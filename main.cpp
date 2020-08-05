
#include "ui_main_window.h"
#include <QApplication>
#include <QTimer>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <jack/jack.h>
#include <mutex>
#include <memory>
#include <cmath>

class SignalView : public QWidget {
public:
    SignalView(QWidget *parent = nullptr);
    void setData(const std::vector<QPointF> &data);
    const std::vector<QPointF> &data() const { return data_; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<QPointF> data_;
};

SignalView::SignalView(QWidget *parent)
    : QWidget(parent)
{
}

void SignalView::setData(const std::vector<QPointF> &data)
{
    data_ = data;
    repaint();
}

void SignalView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    const int w = width();
    const int h = height();

    const QPointF *data = data_.data();
    const int size = data_.size();

    if (size < 1)
        return;

    auto xToCoord = [w](qreal x) -> qreal
    {
        return ((x * (1.0 / 1.5) + 1.0) * 0.5) * (w - 1);
    };
    auto yToCoord = [h](qreal y) -> qreal
    {
        return (1.0 - ((y * (1.0 / 1.5) + 1.0) * 0.5)) * (h - 1);
    };

    painter.setPen(QPen(Qt::gray, 1.0));
    for (qreal x : {-1.0, -0.5, 0.0, 0.5, 1.0})
        painter.drawLine(xToCoord(x), yToCoord(-2), xToCoord(x), yToCoord(2));
    for (qreal y : {-1.0, -0.5, 0.0, 0.5, 1.0})
        painter.drawLine(xToCoord(-2), yToCoord(y), xToCoord(2), yToCoord(y));

    painter.setPen(QPen(Qt::black, 1.0));
    for (int i = 0; i < size; ++i) {
        QPointF p = data[i];
        int x = std::lround(xToCoord(p.x()));
        int y = std::lround(yToCoord(p.y()));
        painter.drawPoint(x, y);
    }
}

///
class Application : public QApplication {
public:
    using QApplication::QApplication;
    ~Application();

    bool init();

private:
    static int process(jack_nframes_t nframes, void *arg);

private:
    std::unique_ptr<Ui::MainWindow> ui_;
    SignalView *view_ = nullptr;

    jack_client_t *client_ = nullptr;
    jack_port_t *port_ref_ = nullptr;
    jack_port_t *port_fx_ = nullptr;

    enum { mem_size = 64 * 1024 };
    size_t mem_index = 0;
    std::vector<QPointF> mem_buf_;
    std::vector<QPointF> mem_ui_;
    std::mutex lock_ui_;
};

Application::~Application()
{
    if (client_)
        jack_client_close(client_);
}

bool Application::init()
{
    setApplicationName("Disto Analyzer");

    mem_buf_.resize(mem_size);
    mem_ui_.resize(mem_size);

    jack_client_t *client = jack_client_open("Disto analyzer", JackNoStartServer, nullptr);
    if (!client)
        return false;

    client_ = client;

    port_ref_ = jack_port_register(client, "Reference", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    port_fx_ = jack_port_register(client, "Effect", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (!port_ref_ || !port_fx_)
        return false;

    jack_set_process_callback(client, &process, this);

    if (jack_activate(client) != 0)
        return false;

    ///

    Ui::MainWindow *ui = new Ui::MainWindow;
    ui_.reset(ui);

    QMainWindow *win = new QMainWindow;
    ui->setupUi(win);

    ui->frmDistoView->setLayout(new QVBoxLayout);
    SignalView *view = new SignalView;
    view_ = view;
    ui->frmDistoView->layout()->addWidget(view);

    win->setWindowTitle(applicationDisplayName());
    win->show();

    QTimer *timer = new QTimer;
    timer->setSingleShot(false);
    timer->setInterval(20);
    timer->start();

    connect(timer, &QTimer::timeout, this, [this, view]()
    {
        std::lock_guard<std::mutex> lock(lock_ui_);
        view->setData(mem_ui_);
    });

    connect(ui->actionSave, &QAction::triggered, this, [this, win, view]()
    {
        std::vector<QPointF> data = view->data();

        QString filename = QFileDialog::getSaveFileName(win, tr("Save data"));
        if (filename.isEmpty())
            return;
        QFile file(filename);
        if (!file.open(QFile::WriteOnly)) {
            QMessageBox::warning(win, tr("Error saving"), tr("Cannot open file for writing."));
            return;
        }

        QTextStream stream(&file);
        for (const QPointF &p : data)
            stream << p.x() << ' ' << p.y() << '\n';
    });

    ///
    return true;
}

int Application::process(jack_nframes_t nframes, void *arg)
{
    Application *app = reinterpret_cast<Application *>(arg);

    const float *ref = reinterpret_cast<float *>(jack_port_get_buffer(app->port_ref_, nframes));
    const float *fx = reinterpret_cast<float *>(jack_port_get_buffer(app->port_fx_, nframes));

    for (size_t i = 0; i < nframes; ++i) {
        app->mem_buf_[app->mem_index] = QPointF(ref[i], fx[i]);
        //app->mem_buf_[app->mem_index] = QPointF(ref[i], fx[i] - ref[i]);
        app->mem_index = (app->mem_index + 1) % mem_size;
    }

    std::unique_lock<std::mutex> lock(app->lock_ui_, std::try_to_lock);
    if (lock.owns_lock())
        std::copy(&app->mem_buf_[0], &app->mem_buf_[mem_size], &app->mem_ui_[0]);

    return 0;
}

///
int main(int argc, char *argv[])
{
    Application app(argc, argv);
    if (!app.init())
        return 1;
    return app.exec();
}
