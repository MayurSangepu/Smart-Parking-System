#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv/cv.h>
#include "opencv/highgui.h"
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);

    ~MainWindow();

    QTimer *t1;
    QTimer *t2;

    
public slots:
    void addListSymbology();

    void createEncode();
    void createEncodeQR();

    void on_ImageButton_clicked();

    void on_VideoButton_clicked();

    void on_CameraButton_clicked();

    void ProcessFrame();

    void showImageEncode();

    void readFromFileText();

    void on_btEncode_clicked();

    void on_pushButton_clicked();

    void on_cbListCode_currentIndexChanged();

    void on_btnSave_clicked();

    void on_pushButton_2_clicked();

    void use_drawRectangle();

    void snake();

    void on_pushButton_3_clicked();

    void on_radioURL_clicked();

    void on_radioText_clicked();

    void on_radioDial_clicked();

    void on_radioSMS_clicked();

    void on_radioContact_clicked();

    void clearAll();
    void disableBarcode();
    void enableQRcode();
    void on_btEncode_2_clicked();

private slots:
    void on_s1open_clicked();

    void on_s1close_clicked();

    void on_s2open_clicked();

    void on_s2close_clicked();

    void on_s3open_clicked();

    void on_s3close_clicked();

    void update1();

    void update2();

private:
    Ui::MainWindow *ui;
    int ScanImage(IplImage *src);
};

#endif // MAINWINDOW_H
