#include <QDebug>
#include <stdio.h>
#include <QMessageBox>
#include <QThread>
#include <QQueue>
#include <QValueAxis>
#include <QLogValueAxis>
#include <QBoxLayout>
#include "data_struct.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QQueue<union block_t>* fifo_ptr , QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    angle.resize(8192);
    I.resize(3);
    gaugeWindow = new GaugeWindow(this);
    fifo = fifo_ptr;

    for(int i=0; i<FFT_N ; i++){
        fft_thread[i] = new QThread();
        fft[i] = new FFTworker(this , i);
        fft[i]->moveToThread(fft_thread[i]);
        samples[i].reserve(FFT_LEN);
    }
    I_chart =   new QChart();
    PI_chart =  new QChart();
    FFT_chart = new QChart();
    IView  =    new QChartView(I_chart);
    PIView =    new QChartView(PI_chart);
    FFTView =   new QChartView(FFT_chart);

    I_chart ->setTitle(QString("XMOS captured sensor data @ %1 kHz").arg(1/dt , 0, 'f' , 2) );
    PI_chart->setTitle(QString("XMOS PI controller @ %1 kHz").arg(1/dt , 0, 'f' , 2) );
    FFT_chart->setTitle(QString("FFT with size %1").arg(FFT_LEN));

    for(int i=IA; i<=Torque ; i++){
        QPointF pnt;
        list[i].reserve(128/DECIMATE*ABUFFERS);
        //Simple low pass filtering
        for(int k=0; k<128/DECIMATE*ABUFFERS ; k++){
            pnt.setX(DECIMATE * (qreal)k * scale.QE);
            pnt.setY((qreal) 0);
            list[i].append(pnt);
        }
        series[i].replace( list[i]);
    }
    //Only use 2 values for setpoint, at least for now
    series[ SetFlux].append(0 , 0);
    series[ SetFlux].append(360,0);
    series[ SetTorque].append(0,0.5);
    series[ SetTorque].append(360,0.5);

    for(int i=IA; i<=IC ; i++){
        series[i].setName(Namestr[i]);
        I_chart ->addSeries( &series[i]);
    }
    for(int i=Flux ; i< len; i++){
        series[i].setName(Namestr[i]);
        PI_chart ->addSeries(&series[i]);
    }

    FFTseries[FFT_IA].setName("I phase A");
    FFTseries[FFT_IC].setName("I phase C");
    QPen pen = FFTseries[FFT_IA].pen();
    pen.setWidth(1);
    pen.setColor(series[IA].color());
    FFTseries[FFT_IA].setPen(pen);
    pen.setColor(series[IC].color());
    FFTseries[FFT_IC].setPen(pen);
    qreal fs=1000/dt;

    FFT_chart->addSeries(&FFTseries[0]);
    FFT_chart->addSeries(&FFTseries[1]);

    I_chart ->createDefaultAxes();
    I_chart-> axisX()->setTitleText("Shaft angle Deg°");
    I_chart-> axisY()->setTitleText("Current [A]");
    I_chart-> axisX()->setRange(0 , 360);
    I_chart-> axisY()->setRange(-2.0 , 2.0);

    PI_chart->createDefaultAxes();
    PI_chart-> axisX()->setTitleText("Shaft angle Deg°");
    PI_chart-> axisY()->setRange(-0.5 , 1.5);
    PI_chart-> axisX()->setRange(0 , 360);


#if(1)
    QLogValueAxis* axisX = new QLogValueAxis();
#else
    QValueAxis* axisX = new QValueAxis();
#endif
    QValueAxis* axisY = new QValueAxis();
    axisX->setLabelFormat("%.0f");
    axisY->setLabelFormat("%.0f");
    axisX->setRange(fs/FFT_LEN  , FFT_PLOT_POINTS*(fs/FFT_LEN));
    axisY->setRange(-40.0 , 80.0);
    axisX->setTitleText("Frequency [Hz]");
    axisY->setTitleText("Level [dB]");

    //axisX->setTickInterval(50.0);
    axisX->setMinorTickCount(3);
    //axisX->setTickCount(10);
    //axisX->applyNiceNumbers();
    axisY->setTickCount(7);
    axisY->setMinorTickCount(3);
    axisY->setTickType(QValueAxis::TickType::TicksFixed);


    FFT_chart->addAxis( axisX , Qt::AlignBottom);
    FFT_chart->addAxis( axisY , Qt::AlignLeft);
    FFTseries[0].attachAxis(axisX);
    FFTseries[0].attachAxis(axisY);
    FFTseries[1].attachAxis(axisX);
    FFTseries[1].attachAxis(axisY);

    //FFT_chart->createDefaultAxes();

     //IView ->setRenderHint(QPainter:: Antialiasing);
     //PIView->setRenderHint(QPainter:: Antialiasing);
     FFTView ->setRenderHint(QPainter:: Antialiasing);

     layout = new QBoxLayout(QBoxLayout::TopToBottom , this);
     layout ->addWidget(IView);
     layout ->addWidget(PIView);
     layout ->addWidget(FFTView);

     box = new QGroupBox("Plots" , this);
     box->setLayout(layout);
     this -> setCentralWidget(box);
     this -> setMinimumWidth(1024);
     this -> setMinimumHeight(768);
     for(int i=0; i<FFT_N ; i++){
        connect(fft[i] , &FFTworker::resultReady , this ,  &MainWindow::update_FFT );
        fft_thread[i]->start();
        }
}

void MainWindow::update_FFT(QVector<QPointF>* freq , int channel){
   FFTseries[channel].replace(*freq);
   FFT_rd_buff = !FFT_rd_buff;
}


void MainWindow::reset_states(){
    for(int i=0; i < FFT_N ; i++)
        fft[i]->rewind();
    FFT_wr_buff=0;
    FFT_rd_buff=0;
}

float MainWindow::filter(qreal x , enum plots_e plot ){
    qreal y;
    const qreal B=1e-3;
    y = B * (x + Xold[plot]) + Yold[plot]*(1-2*B);
    Xold[plot] = x;
    Yold[plot] = y;
    return (float)y;
}

void MainWindow::parse_angle(){
    union block_t block = fifo->dequeue();
    for(int i=0; i<128 ; i++)
        angle.replace(angle_pos++ , block.samples[i]*scale.QE);
    angle_pos &=8191;
}

void::MainWindow::parse_lowspeed(){
    union block_t block = fifo->dequeue();
    float temp = block.lowSpeed.lowspeed.temp;
    //qDebug()<<temp;
    gaugeWindow->setTemp(temp);
}

void MainWindow::parse(enum plots_e plot , enum FFT_e fft_plot , int &index,  bool parseFFT , qreal scale){
    union block_t block = fifo->dequeue();
    int readPos = 0;
    if(parseFFT){
        for(int i=0; i<(128/DECIMATE) ; i++){
            int sum=0;
            for( int d=0; d<DECIMATE ; d++ ){
                qint32 val = block.samples[readPos++];
                sum += val;
                samples[fft_plot].append(val);
            }
            qreal scaled = sum*scale;
            list[plot].value(index).setY(scaled);

            //peak hold
            I[plot].i = abs(scaled);
            if(I[plot].i >= I[plot].peak)
                I[plot].peak = I[plot].i;
            else
                I[plot].peak -=2*(dt/1000); //[A/s]
            I[plot].RMS = filter(scaled*scaled , plot);
        }
    }
    else{
        for(int i=0; i<(128/DECIMATE) ; i++){
            int sum=0;
            for( int d=0; d<DECIMATE ; d++ ){
                sum += block.samples[readPos++];
            }
            list[plot][index++].setY(sum*scale);

        }
    }
}


void MainWindow::update_data(){
      while(fifo->count() >= 8*ABUFFERS){
        for(int i=0; i<len ; i++)
            listIndex[i]=0;

        for(int j=0; j<ABUFFERS ; j++){
            parse_lowspeed();
            parse(IA , FFT_IA , listIndex[IA] , true , scale.Current);
            parse(IC , FFT_IC , listIndex[IC] , true , scale.Current);
            parse_angle();
            parse(Torque , OFF, listIndex[Torque] ,false , scale.Torque);
            parse(Flux   , OFF, listIndex[Flux]   ,false , scale.Flux);
            fifo->removeFirst();//U
            fifo->removeFirst();//ang
        }
        for(int i=0 ; i<(8192/DECIMATE) ; i++)
            list[IB][i].setY(-list[IA][i].y() - list[IC][i].y() );
        for(int i=0; i<(128/DECIMATE) ; i++){ // A already
            qreal Ib = list[IA][i].y() + list[IC][i].y();
            I[IB].RMS = filter(Ib*Ib , IB);
            float absIb= abs(Ib);
            if(absIb >  I[IB].peak)
                I[IB].peak = absIb;
            else
                I[IB].peak -=2*(dt/1000);
        }

        series[IA].replace(list[IA]);
        series[IB].replace(list[IB]);
        series[IC].replace(list[IC]);
        series[Torque].replace(list[Torque]);
        series[Flux].replace(list[Flux]);
        gaugeWindow->setcurrentGauge(I);
        float rpm = (angle[8191]-angle[0])* (60.0f/360.0f/dt/8.192);
        //qDebug()<<rpm;
        gaugeWindow->setShaftSpeed(rpm);
        if(samples[0].size() == FFT_LEN){
            for(int ch=0; ch< FFT_N ; ch++){
                fft_queue[ch].enqueue(samples[ch]);
                fft[ch]->calcFFT(&fft_queue[ch]);
                samples[ch].clear();
            }
        }
    }//while
}

void MainWindow::show_Warning(QString str){
    QMessageBox *msgbox = new QMessageBox(QMessageBox::Warning , "Warning" , str);
     msgbox->show();
}


MainWindow::~MainWindow()
{
    delete ui;
}


