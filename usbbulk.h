#ifndef USBBULK_H
#define USBBULK_H
#include <QObject>
#include <QThread>
#include <QQueue>
#include <QMainwindow>
#include "data_struct.h"
#include "libusb.h"
#include "usb_helper_functions.h"
#include "global_defines.h"
#include "mainwindow.h"
#include "calcfilt.h"



class USBbulk : public QThread {
    Q_OBJECT
public:
    USBbulk(MainWindow* w, QQueue<union block_t> *fifo_ptr);
    ~USBbulk();

signals:
    void dataAvailable();
    void sendWarning(QString str);

public slots:
     void restart_stream(void);
     void start_stream(void);
     void stop_stream(void);
     void sendPIsettings(PI_section_t &PIsection , int channel);
     void sendEQsettings(EQ_section_t &EQ , int channel , int section );


private:
    enum message_e{streamIN , PIsection , EQsection};
    struct USB_PIsection_t{
        int header = USBbulk::PIsection;
        int channel;
        PI_section_t section;
    };

    struct USB_EQsection_t{
        qint32 header = USBbulk::EQsection;
        quint32 channel;
        quint32 section;
        quint32 _padding;
        EQ_section_t data;
    };

    struct tx_t{
        int header;
        int stream;
    };
    void run();
    static void callback(struct libusb_transfer *transfer);
    void testHandleMsg();
    libusb_device **list;
    struct libusb_transfer* In_transfer[BUFFERS] = {nullptr};
    int err = 0;
    struct USBmem_t mem[BUFFERS][ABUFFERS]={0};
    struct libusb_device_handle *handle = NULL;
    int block=0;
    unsigned packets=0;
    int do_exit=0;
    tx_t streamON= {USBbulk::streamIN, true};
    tx_t streamOFF={USBbulk::streamIN, false};
    MainWindow* mainWindow;
    const int VID=0x20b1;
    const int PID=0x00da;
    //struct USBmem_t* buffer;
    const quint64 pi=3141592543358979324ull;
    QQueue<union block_t>* fifo;



};

#endif // USBBULK_H
