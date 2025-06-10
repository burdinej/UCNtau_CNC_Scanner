#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <iostream>
#include <string>
#include <QtDebug>
#include <QByteArray>
#include <unistd.h>
#include <QtMath>
#include <QSerialPort>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    port(new QSerialPort(this))
{
    ui->setupUi(this);


    ui->xPosEdit->setText("0.000");
    double currentX  = 0;
    ui->yPosEdit->setText("0.000");
    double currentY = 0;

    ui->sampleSpacing->setText("5");
    ui->sampleTime->setText("10");
    ui->runTimeEnd->setText("00:00");

    init_port();
}

MainWindow::~MainWindow()
{
    delete ui;
    //port->close(); // Close  arduino serial port on exit
}

//**********************
//Begin Josh's additions
//**********************

//***Command value table***//
//Base state / no move == 0//
//X Back == 1; Y Back == 2 //
//X For == 3; Y For == 4   //
//Run Scan == 5; Stop == 6 //
//Update Position == 7;    //
//Return Home == 8;        //
//Total length = 59cm//
//Total width = 28cm  //
//*************************//

void MainWindow::init_port()
{
    //Set port configuration
    port->setPortName("/dev/ttyACM1");
    port->setBaudRate(QSerialPort::Baud9600);
    port->setFlowControl(QSerialPort::NoFlowControl);
    port->setParity(QSerialPort::NoParity);
    port->setDataBits(QSerialPort::Data8);
    port->setStopBits(QSerialPort::OneStop);

    // Open port
    bool check = false;
    check = port->open(QIODevice::ReadWrite);
    if(!check)
    {
        qDebug() << check;
        qDebug() << port->error();
    }
    else
    {
        qDebug() << check;
    }

    if(!port->isOpen())
    {
        QMessageBox::warning(this, "PORT ERROR", "Arduino port could not be opened!");
    }
}

void MainWindow::transmitVal(char cmd, float val1, float val2)
{
    size_t maxChar = 5;

    std::string valStr = std::to_string(val1);
    std::string valStr2 = std::to_string(val2);
    int maxLength = (valStr.length() < maxChar)?valStr.length():maxChar;
    valStr = valStr.substr(0, maxLength);
    maxLength = (valStr2.length() < maxChar)?valStr2.length():maxChar;
    valStr2 = valStr2.substr(0, maxLength);

    //QString str1 = QString::fromStdString(valStr.c_str());
    //QString str2 = QString::fromStdString(valStr2.c_str());
    //qDebug() << str1;
    //qDebug() << str2;

    int strLen = valStr.length();
    int strLen2 = valStr2.length();
    char tempBuf[strLen + 1]; // temp buff for str to char array
    char tempBuf2[strLen2 + 1];
    char buffer[15] = {}; // buffer to hold up to 15 char (start, end, 2 comma, and 11 data char)
    size_t i = 0;

    strcpy(tempBuf, valStr.c_str());
    strcpy(tempBuf2, valStr2.c_str());
    buffer[0] = '<';
    buffer[1] = cmd;
    buffer[2] = ',';

    for (i = 0; i < (sizeof(tempBuf) / sizeof(tempBuf[0])); i++)
    {
        buffer[i + 3] = tempBuf[i];
    }
    buffer[strLen + 3] = ',';

    for (i = 0; i < (sizeof(tempBuf2) / sizeof(tempBuf2[0])); i++)
    {
        buffer[i + strLen + 4] = tempBuf2[i];
    }
    buffer[strLen + strLen2 + 4] = '>';

    if (port->isOpen())
    {
        port->write(buffer);
    }
    else
    {
        QMessageBox::warning(this, "PORT ERROR", "Arduino port is not open!");
    }
}

void MainWindow::updatePosDisplay()
{
    double xInCm = 0;
    double yInCm = 0;

    xInCm = (currentX / usteps) / (71.0 + (15.0 / 32.0));
    yInCm = (currentY / usteps) / (71.0 + (5.0 / 32.0));

    QString newX = QString::number(xInCm, 'f', 3);
    ui->xPosEdit->setText(newX);

    QString newY = QString::number(yInCm, 'f', 3);
    ui->yPosEdit->setText(newY);

    ui->posUpdate->setEnabled(true);
    ui->returnHome->setEnabled(true);
    ui->runScan->setEnabled(true);
    ui->stopRun->setEnabled(true);
}

double MainWindow::calcTime()
{
    float spacing = 0.0;
    float timing = 0.0;
    double timeOut = 0.0;
    double timeIn = 0.0;
    double timeUp = 0.0;
    double totalTime = 0.0; // total time for scan in minutes (min)
    double minPerRot = 0.0;
    double sec2min = 0.0;
    double cmToStepsWid = 0.0;
    double cmToStepsLen = 0.0;
    double lengthUSteps = 0.0;
    double widthUSteps = 0.0;


    QString sampleTiming = ui->sampleTime->text();
    timing = sampleTiming.toDouble();
    sec2min = timing / 60.0;

    QString sampleSpace = ui->sampleSpacing->text();
    spacing = sampleSpace.toDouble();
    cmToStepsWid = spacing * (71.0 + (5.0 / 32.0));
    cmToStepsLen = spacing * (71.0 + (15.0 / 32.0));
    lengthUSteps = cmToStepsLen * usteps;
    widthUSteps = cmToStepsWid * usteps;

    minPerRot = 1.0 / RPM;

    timeOut = ((qFloor((MAX_STEPS_WIDTH * usteps) / widthUSteps) + 1) * sec2min)
              + ((minPerRot * (widthUSteps / (200 * usteps))) * qFloor((MAX_STEPS_WIDTH * usteps) / widthUSteps));

    timeIn = (((qFloor(((MAX_STEPS_WIDTH * usteps) / widthUSteps)) * widthUSteps) / (200 * usteps)) * minPerRot);

    timeUp = (((MAX_STEPS_LENGTH * usteps) / lengthUSteps) * (lengthUSteps / (200 * usteps)) * minPerRot);

    totalTime = (timeOut + timeIn) * (((MAX_STEPS_LENGTH * usteps) / lengthUSteps) + 1) + timeUp;

    return totalTime;
}

void MainWindow::on_runScan_clicked()
{
    ui->posUpdate->setEnabled(true);
    ui->returnHome->setEnabled(true);
    ui->runScan->setEnabled(false);
    ui->stopRun->setEnabled(true);
    float spacing;
    float timing;
    double runTime = 0.0;
    double curHour = 0.0;
    double curMin = 0.0;
    double curSec = 0.0;
    double decTime = 0.0;
    double newTime = 0.0;
    int newHour = 0;
    int newMin = 0;

    command = '5';
    QString string1 = "0";
    QByteArray stopCmd = string1.toUtf8();
    breakFlag = false;

    QString sampleSpace = ui->sampleSpacing->text();
    spacing = sampleSpace.toDouble();

    QString sampleTiming = ui->sampleTime->text();
    timing = sampleTiming.toDouble();

    runTime = calcTime();
    QTime time = QTime::currentTime();
    curHour = time.hour();
    curMin = time.minute();
    curSec = time.second();
    decTime = (curHour * 60.0) + curMin + (curSec / 60.0);

    newTime = decTime + runTime;
    newHour = newTime / 60;
    newMin = ((newTime / 60.0) - newHour) * 60;
    //qDebug() << newHour;
    //qDebug() << newMin;

    std::string hour = std::to_string(newHour);
    std::string min = std::to_string(newMin);
    std::string timeNew = hour + ":" + min;
    QString qtime = QString::fromStdString(timeNew);

    ui->runTimeEnd->setText(qtime);

    if ((currentX == 0) & (currentY == 0))
    {
        if (spacing > 28.000)
        {
            QMessageBox::warning(this, "Error Scanning", "Sample spacing must be less than 28.000 cm");
        }
        else
        {
            transmitVal(command, spacing, timing);

            ui->xPosEdit->setText("0.000");
            ui->yPosEdit->setText("0.000");
            currentX = 0;
            currentY = 0;

            // qDebug() << "Text Value Set";
            // qDebug() << stopCmd << stopCmd.size();

            while((stopCmd[0] != '9') & (breakFlag == false))
            {                
                QCoreApplication::processEvents();
                if (port->waitForReadyRead(10000))
                {
                    stopCmd = port->readAll();
                }
                // qDebug() << "stopCmd: " << stopCmd;
            }

            // qDebug() << "While Loop Ended";

            ui->runScan->setEnabled(true);
            ui->runTimeEnd->setText("00:00");
        }
    }
    else
    {
        QMessageBox::warning(this, "Error Scanning", "You must start at home (0,0)!!");
    }

}

void MainWindow::on_posUpdate_clicked()
{
    ui->returnHome->setEnabled(true);
    ui->runScan->setEnabled(true);
    ui->stopRun->setEnabled(true);

    double xPosDesire;
    double yPosDesire;
    command = '7';

    QString xPosString = ui->xPosEdit->text();
    QString yPosString = ui->yPosEdit->text();
    xPosDesire = xPosString.toDouble();
    yPosDesire = yPosString.toDouble();
    if ((xPosDesire >= 0) & (yPosDesire >= 0))
    {
        if ((xPosDesire > 59.000) | (yPosDesire > 28.000))
        {
            QMessageBox::warning(this, "Position Update Error", "New position must be less than 59.000 cm for X and 28.000 cm for Y!");
        }
        else
        {
            transmitVal(command, xPosDesire, yPosDesire);

            currentX = xPosDesire * (71.0 + (15.0 / 32.0)) * usteps;
            currentY = yPosDesire * (71.0 + (5.0 / 32.0)) * usteps;
            qDebug("%f usteps", currentX);
            qDebug("%f usteps", currentY);
        }
    }
    else
    {
        QMessageBox::warning(this, "Position Update Error", "New position must be greater than or equal to (0,0)!!");
    }

}

void MainWindow::on_returnHome_clicked()
{
    ui->posUpdate->setEnabled(true);
    ui->returnHome->setEnabled(false);
    ui->runScan->setEnabled(true);
    ui->stopRun->setEnabled(false);
    command = '8';

    transmitVal(command, 0, 0);
    ui->xPosEdit->setText("0.000");
    ui->yPosEdit->setText("0.000");
    currentX = 0.0;
    currentY = 0.0;
}

void MainWindow::on_stopRun_clicked()
{
    // if (breakFlag == true)
    // {
    //     return;
    // }

    breakFlag = true;

    ui->posUpdate->setEnabled(true);
    ui->returnHome->setEnabled(true);
    ui->runScan->setEnabled(true);
    ui->stopRun->setEnabled(false);

    command = '6';

    ui->runTimeEnd->setText("00:00");
    transmitVal(command, 0, 0);
    char zero[] = {'0'};
    QByteArray data(QByteArray::fromRawData(zero, 1));

    qDebug() << "data: " << data;

    QString string1 = "0";
    QByteArray stopCmd = string1.toUtf8();
    // QByteArray stopCmd(QByteArray::fromRawData(zero, 1));

    qDebug() << "stopCmd: " << stopCmd;

    int ndx = 0;
    int i = 0;
    char curXArray[15] = {'0'};
    char curYArray[15] = {'0'};

    if (port->waitForReadyRead(10000))
    {
        // usleep(30000);
        usleep(2000000);//1000000 is second
        stopCmd = port->readAll();
    }

    qDebug() << "stopCmd after if: " << stopCmd;

    qDebug() << "Initialized";

    if (stopCmd[0] == '9')
    {
        //on_stopButton_clicked();
    }
    else if (stopCmd[0] == '0')
    {
        // data acq was never started (this was not a run scan stop)
    }
    else
    {
        std::cout << "Stop command not receive. Data aqcuisition continues." << std::endl;
    }

    if (port->waitForReadyRead(10000)) // read pos update
    {
        // usleep(5000000); //1.2 sec
        data = port->readAll();
    }

    qDebug() << "data after if: " << data;

    while (data[ndx] != '>')
    {
        curXArray[i] = data[ndx + 1];
        ndx++;
        i++;
    }
    curXArray[i - 1] = '\0';
    ndx++;
    i = 0;

    while (data[ndx] != '>')
    {
        curYArray[i] = data[ndx + 1];
        ndx++;
        i++;
    }
    curYArray[i - 1] = '\0';

    currentX = atof(curXArray);
    currentY = atof(curYArray);

    qDebug("%f usteps", currentX);
    qDebug("%f usteps", currentY);
    updatePosDisplay();

    qDebug() << "stop done";

}

void MainWindow::on_xBack_clicked()
{

    command = '1';
    if (currentX > 0)
    {
        transmitVal(command, 0, 0);

        currentX = currentX - usteps;
        updatePosDisplay();
    }
    else
    {
        QMessageBox::warning(this, "Take Step Error", "At min position, can't step back!");
    }
}

void MainWindow::on_yBack_clicked()
{

    command = '2';
    if (currentY > 0)
    {
        transmitVal(command, 0, 0);

        currentY = currentY - usteps;
        updatePosDisplay();
    }
    else
    {
        QMessageBox::warning(this, "Take Step Error", "At min position, can't step back!");
    }
}

void MainWindow::on_xFor_clicked()
{

    command = '3';
    if (currentX < (4214.8215 * usteps))
    {
        transmitVal(command, 0, 0);

        currentX = currentX + usteps;
        updatePosDisplay();
    }
    else
    {
        QMessageBox::warning(this, "Take Step Error", "At max position, can't step forward!");
    }
}

void MainWindow::on_yFor_clicked()
{

    command = '4';
    if (currentY < (1992.375 * usteps))
    {
        transmitVal(command, 0 , 0);

        currentY = currentY + usteps;
        updatePosDisplay();
    }
    else
    {
        QMessageBox::warning(this, "Take Step Error", "At max position, can't step forward!");
    }
}

void MainWindow::on_testSerial_clicked()
{
    for (int i = 0; i < 10; ++i){
        if (port->isOpen())
        {
            QString string1 = QString::number(i);
            QByteArray test = string1.toUtf8();
            auto result = port->write(test);
            if (result == -1) {
                qDebug() << "Failed to write to port" << port->errorString();
                return;
            } else if (result != test.size()) {
                qDebug() << "Failed to write all data to port";
                return;
            } else {
                qDebug() << result << "bytes written:" << test;
            }
            if (!port->waitForBytesWritten(1000)) {
                qDebug() << "Write timed out";
                return;
            }
        }
        else
        {
            QMessageBox::warning(this, "PORT ERROR", "Arduino port is not open!");
            return;
        }

        if (port->waitForReadyRead(5000))
        {
            QByteArray charthing = port->readAll();
            QString charthing2 = QString::fromUtf8(charthing);
            qDebug() << "Response: " << charthing2;
        } else {
            qDebug() << "Read timed out";
            return;
        }
    }
}

