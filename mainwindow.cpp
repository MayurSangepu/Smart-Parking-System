#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "zbar.h"
#include "zbar/Image.h"
#include "zbar/ImageScanner.h"
#include "QFileDialog"
#include "zbar/Video.h"
#include "QMessageBox"
#include "QWidget"
#include "QTimer"
#include <zint.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/legacy/legacy.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <QDebug>
#include <QTime>
#include <QProcess>
#include <QtSql/QSqlError>
#include <QtSql/QtSql>
#include <QtSql/QSqlQuery>
#include <QDebug>
#include <QMessageBox>
#include <QtSql/QSqlDatabase>
#include <wiringPi.h>
#include <QTimer>

using namespace zbar;
using namespace cv;

QString s;


typedef struct parameter Parameter;
struct parameter {
    float alpha;
    float beta;
    float gamma;
};

int thresh = 100;
int max_thresh = 255;
RNG rng(12345);
Mat src_gray;

struct zint_symbol *my_symbol;

QString strFilePath;
CvCapture* capture;
QTimer *timer;
int symbology=1;
void thresh_callback(int, void* );

int s1oper=0;
int s2oper=0;
int s3oper=0;

int slot1val;
int slot2val;
int slot3val;

int slot1upload=0;
int slot2upload=0;
int slot3upload=0;

QString slot1data;
QString slot2data;
QString slot3data;

QString nildata="NIL";


void delay()
{
    QTime dieTime= QTime::currentTime().addSecs(5);
    while(QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);

}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),

    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
     wiringPiSetup();

    ui->ImageButton->setVisible(false);
    ui->CameraButton->setVisible(false);
    ui->VideoButton->setVisible(false);


    addListSymbology();

    pinMode(29,OUTPUT); // BUZZER
    digitalWrite(29,0);


    pinMode(23,INPUT); // IR 1
    pinMode(24,INPUT); // IR 2
    pinMode(25,INPUT); // IR 3

    digitalWrite(23,1);
    digitalWrite(24,1);
    digitalWrite(25,1);


    QSqlDatabase db=QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("SmartParkingSystem");
    db.setPort(3306);
    db.setDatabaseName("smart_parking_system");
    db.setUserName("smartparking");
    db.setPassword("smart_parking");
    if (!db.open())
    {
        qDebug() << "Database error occurred"<<db.lastError();
    }
    else
    {
        qDebug()<<"Database connected";
    }

    // on_radioText_clicked();

    t1=new QTimer(this);
    connect(t1,SIGNAL(timeout()),this,SLOT(update1()));
    t1->start(1000);

    t2=new QTimer(this);
    connect(t2,SIGNAL(timeout()),this,SLOT(update2()));
    t2->start(1000);

    on_CameraButton_clicked();

}
void MainWindow::addListSymbology()
{

    ui->cbListCode->removeItem(0);
    ui->cbListCode->addItem("Standard Code 2 of 5");    //2
    ui->cbListCode->addItem("Interleaved 2 of 5");      //3
    ui->cbListCode->addItem("Code 3 of 9 (Code 39)");   //8
    ui->cbListCode->addItem("Extended Code 3 of 9 (Code 39+)"); //9
    ui->cbListCode->addItem("EAN");
    ui->cbListCode->addItem("UPC A");
    ui->cbListCode->addItem("UPC E");
    ui->cbListCode->addItem("PDF417");
    ui->cbListCode->addItem("Code 128");
    ui->cbListCode->addItem("QRCODE");
}

MainWindow::~MainWindow()
{
    // Release the capture device housekeeping
    cvReleaseCapture( &capture );
    delete ui;

}

void MainWindow::on_ImageButton_clicked()
{
    cvReleaseCapture;
    int n;

    strFilePath = QFileDialog::getOpenFileName(this, tr("Open a image"), QDir::currentPath(), tr("Image Files (*.jpg *.jpeg *.png)"));
    if(strFilePath.size())
    {

        IplImage *src = cvLoadImage((const char*)strFilePath.toAscii().data(),CV_LOAD_IMAGE_COLOR);

        // Create a QImage to show the captured images
        QImage img_show = QImage((const unsigned char*)(src->imageData),src->width,src->height,QImage::Format_RGB888).rgbSwapped();
        ui->showlabel->setPixmap(QPixmap::fromImage(img_show,Qt::AutoColor).scaledToWidth(300));
        n=ScanImage(src);
        //if(n>0)
        {
            //  cvReleaseCapture(&capture);
        }
    }
}
void MainWindow::on_VideoButton_clicked()
{

    strFilePath = QFileDialog::getOpenFileName(this, tr("Open a video"), QDir::currentPath(), tr("Video Files (*.mp4 *.flv *.wmv)"));
    capture=cvCaptureFromFile(strFilePath.toAscii().data());
    if(capture)
    {
        timer = new QTimer(this);
        QObject::connect(timer,SIGNAL(timeout()),this,SLOT(ProcessFrame()));

        timer->start(1000/30);
    }
    else ui->textResult->setText("Cannot open a Video!");
}

void MainWindow::on_CameraButton_clicked()
{
    //Kiem tra trang thai bat tat cua camera
    if(capture == 0)
    {
        // capture = cvCaptureFromCAM(CV_CAP_ANY);
        capture = cvCaptureFromCAM(0);
        if(capture)
        {
            //cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT,1600);
            //cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH,1600);

            timer = new QTimer(this);

            QObject::connect(timer,SIGNAL(timeout()),this,SLOT(ProcessFrame()));

            timer->start(1000/30);
        }
        else {
            //ui->textResult->setText("Cannot connect to Camera!");
        }
    }

    return ;
}

int MainWindow::ScanImage(IplImage *src)
{

    IplImage *img = cvCreateImage(cvSize(src->width,src->height), IPL_DEPTH_8U,1);

    cvCvtColor( src, img, CV_RGB2GRAY );

    //Su dung thu vien Zbar de giai ma

    uchar* raw = (uchar *)img->imageData;
    int width = img->width;
    int height = img->height;

    ImageScanner *scanner=new ImageScanner();

    scanner->set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);

    Image *zimg=new Image(width,height,"Y800",raw,width*height);

    int n=scanner->scan(*zimg);

    if(n>0)
    {
        for(Image::SymbolIterator symbol = zimg->symbol_begin(); symbol != zimg->symbol_end(); ++symbol)
        {

            //  ui->textResult->setPlainText(QString::fromStdString(symbol->get_data()));
            s=QString::fromStdString(symbol->get_data());
            qDebug() << "----DATA :---- " <<  s;
        }

    }

    else
    {
        // ui->textResult->setPlainText("Cannot Detect Code!");
    }

    zimg->set_data(NULL, 0);

    return n;
}
void MainWindow::ProcessFrame()
{
    IplImage* src = cvQueryFrame(capture);
    QImage img_show = QImage((unsigned char*)(src->imageData),src->width,src->height,QImage::Format_RGB888).rgbSwapped();
    ui->showlabel->setPixmap(QPixmap::fromImage(img_show,Qt::AutoColor).scaledToWidth(300));

    int n = ScanImage(src);

    if(n)
    {
        timer->stop();
        QStringList ds= s.split("|",QString::SkipEmptyParts);

        qDebug() << "**************DATA2 : " <<  ds;

        ui->textResult->setText(ds.at(0));

        if(ds.at(1)=="1"){
            qDebug() << "---QR SLOT 1----  ";
            ui->parkmsg->setText(" PARK VEHICLE AT ");
            ui->slotmsg->setText(" SLOT : 1 ");
            on_s1open_clicked();
            s1oper=1;
        }
        else if(ds.at(1)=="2"){
            qDebug() << "---QR SLOT 2----  ";
            ui->parkmsg->setText(" PARK VEHICLE AT ");
            ui->slotmsg->setText(" SLOT : 2 ");
            on_s2open_clicked();
            s2oper=1;
        }
        else if(ds.at(1)=="3"){
            qDebug() << "---QR SLOT 3----  ";
            ui->parkmsg->setText(" PARK VEHICLE AT ");
            ui->slotmsg->setText(" SLOT : 3 ");
            on_s3open_clicked();
            s3oper=1;
        }
        delay();

        ui->textResult->clear();
        ui->parkmsg->clear();
        ui->slotmsg->clear();

        cvReleaseCapture(&capture);
        on_CameraButton_clicked();
    }
}

//////////////////////////////////////////////////////
//Encoder text -> Image;
void MainWindow::createEncode()
{
    QString content="";
    my_symbol = ZBarcode_Create();
    my_symbol->border_width=10;
    my_symbol->symbology=symbology;
    //my_symbol->scale=5.0;
    if(my_symbol != NULL)
    {
        content = ui->textBarcode->toPlainText();
        unsigned char * sequence=NULL;
        sequence=(unsigned char*)qstrdup(content.toAscii().constData());

        //strcpy(my_symbol->outfile,(char*)qstrdup(content.toAscii().constData()));

        ZBarcode_Encode_and_Print(my_symbol,sequence,0,0);
    }
    ZBarcode_Delete(my_symbol);

    showImageEncode();
}
void MainWindow::createEncodeQR()
{
    QString content="";
    my_symbol = ZBarcode_Create();
    my_symbol->border_width=10;
    my_symbol->symbology=symbology;
    my_symbol->scale=5.0;
    if(my_symbol != NULL)
    {
        content = ui->textResult_2->toPlainText()+" "+ui->textResult_3->toPlainText()+" "+ui->textResult_4->toPlainText();
        unsigned char * sequence=NULL;
        sequence=(unsigned char*)qstrdup(content.toAscii().constData());

        //strcpy(my_symbol->outfile,(char*)qstrdup(content.toAscii().constData()));

        ZBarcode_Encode_and_Print(my_symbol,sequence,0,0);
    }
    ZBarcode_Delete(my_symbol);

    showImageEncode();
}

void MainWindow::on_btEncode_clicked()
{
    if(symbology!=58)
    {
        createEncode();
    }
    else
    {
        createEncodeQR();
    }

}


void MainWindow::showImageEncode()
{
    QString path = QDir::currentPath()+"/out.png";

    IplImage *src = cvLoadImage((const char*)path.toAscii().data(),CV_LOAD_IMAGE_COLOR);

    QImage img_show = QImage((const unsigned char*)(src->imageData),src->width,src->height,QImage::Format_RGB888).rgbSwapped();
    ui->showlabel_2->setPixmap(QPixmap::fromImage(img_show,Qt::AutoColor).scaledToWidth(185));
}

void MainWindow::readFromFileText()
{
    struct zint_symbol * barcode;
    barcode=ZBarcode_Create();
    strFilePath=QFileDialog::getOpenFileName(this,tr("Open file text"),QDir::currentPath(),tr("Text file(*.txt)"));
    char * sequence=NULL;
    sequence=(char*)qstrdup(strFilePath.toAscii().constData());
    if(barcode!=NULL)
    {
        ZBarcode_Encode_File_and_Print(barcode,sequence,0);
        // ZBarcode_Encode(barcode,sequence,10);
    }
    ZBarcode_Delete(barcode);
}

void MainWindow::on_pushButton_clicked()
{
    readFromFileText();
    showImageEncode();
}

void MainWindow::disableBarcode()
{
    ui->textBarcode->show();
    ui->groupQr1->hide();
    ui->groupQr2->setTitle("Input barcode:");
    ui->label_content->hide();
}
void MainWindow::enableQRcode()
{
    ui->groupQr1->show();
    ui->groupQr2->setTitle("Content:");
    ui->label_content->show();
    ui->textBarcode->hide();
}
void MainWindow::on_cbListCode_currentIndexChanged()
{
    switch (ui->cbListCode->currentIndex())
    {
    case 0: {symbology=2;disableBarcode();break;}
    case 1: {symbology=3;disableBarcode();break;}
    case 2: symbology=8;disableBarcode();break;
    case 3: symbology=9;disableBarcode();break;
    case 4: symbology=13;disableBarcode();break;
    case 5: symbology=34;disableBarcode();break;
    case 6: symbology=37;disableBarcode();break;
    case 7: symbology=55;disableBarcode();break;
    case 8: symbology=60;disableBarcode();break;
    case 9:
    {
        symbology=58;
        enableQRcode();
        break;
    }
    }
}

void MainWindow::on_btnSave_clicked()
{
    strFilePath = QFileDialog::getOpenFileName(this, tr("Open a video"), QDir::currentPath(), tr("Video Files (*.mp4 *.flv *.wmv)"));
    capture=cvCaptureFromFile(strFilePath.toAscii().data());
    if(capture)
    {
        timer = new QTimer(this);
        QObject::connect(timer,SIGNAL(timeout()),this,SLOT( use_drawRectangle()));

        timer->start(1000/30);
    }
    else ui->textResult->setText("Cannot Open a Video!");

}

void MainWindow::on_pushButton_2_clicked()
{
    strFilePath = QFileDialog::getOpenFileName(this, tr("Open a image"), QDir::currentPath(), tr("Image Files (*.jpg *.jpeg *.png)"));

    if(strFilePath.size())
    {
        cv::Mat test;

        char * sequence=NULL;
        sequence=(char*)qstrdup(strFilePath.toAscii().constData());
        test=cv::imread(sequence);
        //test=cv::imread(strFilePath);

        /// Convert it to gray,blue
        cvtColor( test, src_gray, CV_BGR2GRAY );
        blur(src_gray,src_gray,Size(1,1));
        /// Create Window
        char* source_window = "Source";
        namedWindow( source_window, CV_WINDOW_NORMAL );
        imshow( source_window, test );
        createTrackbar( " Threshold:", "Source", &thresh, max_thresh,thresh_callback);
        thresh_callback( 0, 0);

    }
}
void thresh_callback(int, void*)
{

    //Mat src_copy = src.clone();
    Mat threshold_output;
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;
    /// Detect edges using Threshold
    threshold( src_gray, threshold_output, thresh, 255, THRESH_BINARY );
    /// Find contours
    findContours( threshold_output, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );
    /// Find the convex hull object for each contour
    vector<vector<Point> >hull( contours.size() );
    for( int i = 0; i < contours.size(); i++ )
    { convexHull( Mat(contours[i]), hull[i], false ); }
    /// Draw contours + hull results
    Mat drawing = Mat::zeros( threshold_output.size(), CV_8UC3 );
    for( int i = 0; i< contours.size(); i++ )
    {
        Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
        drawContours( drawing, contours, i, color, 1, 8, vector<Vec4i>(), 0, Point() );
        drawContours( drawing, hull, i, color, 1, 8, vector<Vec4i>(), 0, Point() );
    }
    /// Show in a window
    namedWindow( "Hull demo", CV_WINDOW_NORMAL);
    imshow( "Hull demo", drawing );

}

void MainWindow::use_drawRectangle()
{
    IplImage *src_img = 0, *dst_img;
    IplImage *src_img_gray = 0;
    IplImage *tmp_img;
    CvMemStorage *storage = cvCreateMemStorage (0);
    CvSeq *contours = 0;
    CvBox2D ellipse;
    CvTreeNodeIterator it;
    CvPoint2D32f pt[4];

    IplImage* src = cvQueryFrame(capture);


    src_img=src;

    src_img_gray = cvCreateImage (cvGetSize (src_img), IPL_DEPTH_8U, 1);
    cvCvtColor (src_img, src_img_gray, CV_BGR2GRAY);
    tmp_img = cvCreateImage (cvGetSize (src_img), IPL_DEPTH_8U, 1);
    dst_img = cvCloneImage (src_img);

    cvThreshold (src_img_gray, tmp_img, 100, 255, CV_THRESH_BINARY);
    cvFindContours (tmp_img, storage, &contours, sizeof (CvContour),
                    CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint (0, 0));

    cvInitTreeNodeIterator (&it, contours, 3);

    while ((contours = (CvSeq *) cvNextTreeNode (&it)) != NULL)
    {
        if (contours->total > 5) {

            ellipse = cvFitEllipse2 (contours);
            ellipse.angle = 90.0 - ellipse.angle;

            cvDrawContours (dst_img, contours, CV_RGB (255, 0, 0), CV_RGB (255, 0, 0), 0, 1, CV_AA, cvPoint (0, 0));
            //cvEllipseBox (dst_img, ellipse, CV_RGB (0, 0, 255), 2);
            //cvBoxPoints (ellipse, pt);
            cvLine (dst_img, cvPointFrom32f (pt[0]), cvPointFrom32f (pt[1]), CV_RGB (0, 255, 0));
            cvLine (dst_img, cvPointFrom32f (pt[1]), cvPointFrom32f (pt[2]), CV_RGB (0, 255, 0));
            cvLine (dst_img, cvPointFrom32f (pt[2]), cvPointFrom32f (pt[3]), CV_RGB (0, 255, 0));
            cvLine (dst_img, cvPointFrom32f (pt[3]), cvPointFrom32f (pt[0]), CV_RGB (0, 255, 0));
        }

    }

    //cvNamedWindow ("Fitting", CV_WINDOW_NORMAL);
    //cvShowImage ("Fitting", dst_img);

    QImage img_show = QImage((unsigned char*)(dst_img->imageData),dst_img->width,dst_img->height,QImage::Format_RGB888).rgbSwapped();
    ui->showlabel->setPixmap(QPixmap::fromImage(img_show,Qt::AutoColor).scaledToWidth(300));
    int n = ScanImage(dst_img);

    if(n)
    {
        timer->stop();
        cvReleaseCapture(&capture);
    }

    /*

      cvWaitKey (0);
      cvDestroyWindow ("Fitting");
      cvReleaseImage (&src_img);
      cvReleaseImage (&dst_img);
      cvReleaseImage (&src_img_gray);
      cvReleaseImage (&tmp_img);*/
    cvReleaseMemStorage (&storage);


}
void MainWindow::snake()
{


}
void MainWindow::on_pushButton_3_clicked()
{
    strFilePath = QFileDialog::getOpenFileName(this, tr("Open a video"), QDir::currentPath(), tr("Video Files (*.mp4 *.flv *.wmv)"));
    capture=cvCaptureFromFile(strFilePath.toAscii().data());
    if(capture)
    {
        timer = new QTimer(this);
        QObject::connect(timer,SIGNAL(timeout()),this,SLOT(snake()));

        timer->start(1000/30);
    }
    else ui->textResult->setText("Cannot connect to Camera!");
}

void MainWindow::on_radioURL_clicked()
{
    ui->label_content->setText("URL:");
    ui->textResult_2->setText("http://");
    ui->textResult_2->resize(341,30);
    ui->textResult_3->hide();
    ui->label_content_2->hide();
    ui->label_content_3->hide();
    ui->textResult_4->hide();
}

void MainWindow::on_radioText_clicked()
{

    ui->textResult_2->setText("");
    ui->label_content->setText("Text:\t\t\t (250 Character left)");
    ui->textResult_2->resize(341,100);
    ui->textResult_3->hide();
    ui->label_content_2->hide();
    ui->label_content_3->hide();
    ui->textResult_4->hide();
}

void MainWindow::on_radioDial_clicked()
{
    ui->textResult_2->setText("");
    ui->label_content->setText("Dial:");
    ui->textResult_2->resize(341,30);
    ui->textResult_3->hide();
    ui->label_content_2->hide();
    ui->label_content_3->hide();
    ui->textResult_4->hide();

}

void MainWindow::on_radioSMS_clicked()
{
    ui->label_content->setText("Dial:");
    ui->textResult_2->setText("");
    ui->textResult_2->resize(341,30);
    ui->textResult_3->show();
    ui->label_content_2->show();
    ui->label_content_2->setText("Message:");
    ui->textResult_3->resize(341,60);
    ui->label_content_3->hide();
    ui->textResult_4->hide();
}

void MainWindow::on_radioContact_clicked()
{
    ui->label_content->setText("Name:");
    ui->textResult_2->setText("");
    ui->textResult_2->resize(341,30);
    ui->textResult_3->show();
    ui->label_content_2->show();
    ui->label_content_2->setText("Tel:");
    ui->textResult_3->resize(341,30);
    ui->label_content_3->show();
    ui->label_content_3->setText("Email:");
    ui->textResult_4->show();
    ui->textResult_4->resize(341,30);
}
void MainWindow::clearAll()
{
    ui->textResult_2->setText("");
    ui->textResult_3->setText("");
    ui->textResult_4->setText("");
}

void MainWindow::on_btEncode_2_clicked()
{
    //QMessageBox::information(this,"", QFileDialog::getSaveFileName(this,tr("Save File"),QDir::currentPath(),tr("Files Format (*.png)")));
    strFilePath = QFileDialog::getSaveFileName(this,tr("Save File"),QDir::currentPath(),tr("PNG (*.png)"));
    char *m="agfdsaf";
    strcpy(strFilePath.toAscii().data() ,m);
}

void MainWindow::on_s1open_clicked()
{
    qDebug() << "SLOT 1 GATE OPEN";
    system("python slot1open.py");
}

void MainWindow::on_s1close_clicked()
{
    qDebug()<< "SLOT 1 GATE CLOSE";
    system("python slot1close.py");
}

void MainWindow::on_s2open_clicked()
{
    qDebug() << "SLOT 2 GATE OPEN";
    system("python slot2open.py");
}

void MainWindow::on_s2close_clicked()
{
    qDebug()<< "SLOT 2 GATE CLOSE";
    system("python slot2close.py");
}

void MainWindow::on_s3open_clicked()
{
    qDebug() << "SLOT 3 GATE OPEN";
    system("python slot3open.py");
}

void MainWindow::on_s3close_clicked()
{
    qDebug()<< "SLOT 3 GATE CLOSE";
    system("python slot3close.py");
}


/****************************SQL AND IR UPDATE******************************/

void MainWindow::update1()
{
    QSqlQuery query;

    query.exec("SELECT sone,stwo,sthree FROM `parkingdata` WHERE 1");;

    if (!query.isActive())
        QMessageBox::warning(this, tr("Database Error"),query.lastError().text());

    while(query.next())
    {

        //int name = query.value(0).toInt();
        qDebug() << "SLOT 1: " << query.value(0).toInt()
                 << "---"
                 <<"SLOT 2: "<<query.value(1).toInt()
                << "---"
                << "SLOT 3: "<<query.value(2).toInt();
    }
}

void MainWindow::update2()
{


    slot1val=digitalRead(23);
    qDebug() << "//////SLOT 1 IR VALUE:////:  " << slot1val;

    if(slot1val==0){

        if(slot1upload==0){
            slot1upload=1;
            qDebug() << "******** SLOT 1 OCCUPIED **************** " ;
            qDebug() << "//////////// SLOT 1 IR update to server///////////";

            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET sone=3 WHERE 1");

        }

    }

    else {
        if(slot1upload==1){
            qDebug() << "******** SLOT 1 IR FREE AFTER OCCUPIED **************** " ;
            slot1upload=0;
//            if(s1oper==1){
//                s1oper=0;
//                on_s1close_clicked();
//            }


            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET sone=1 WHERE 1");
            on_s1close_clicked();
        }

    }


    slot2val = digitalRead(24);
    qDebug() << "SLOT 2 IR VALUE----" << slot2val;

    if(slot2val==0){

        if(slot2upload==0){
            slot2upload=1;
            qDebug() << "******** SLOT 2 OCCUPIED **************** " ;
            qDebug() << "//////////// SLOT 2 IR update to server///////////";
            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET stwo=3 WHERE 1");

        }

    }

    else {
        if(slot2upload==1){
            qDebug() << "******** SLOT 2 IR FREE AFTER OCCUPIED **************** " ;
            slot2upload=0;
            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET stwo=1 WHERE 1");
            on_s2close_clicked();

        }

    }




    slot3val = digitalRead(25);
    qDebug() << "******SLOT 3 IR VALUE" << slot3val;

    if(slot3val==0){

        if(slot3upload==0){
            slot3upload=1;
            qDebug() << "******** SLOT 3 OCCUPIED **************** " ;
            qDebug() << "//////////// SLOT 3 IR update to server///////////";
            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET sthree=3 WHERE 1");

        }

    }

    else {
        if(slot3upload==1){
            qDebug() << "******** SLOT 3 IR FREE AFTER OCCUPIED **************** " ;
            slot3upload=0;
            QSqlQuery squery;
            squery.exec("UPDATE parkingdata SET sthree=1 WHERE 1");
            on_s3close_clicked();

        }

    }
}

