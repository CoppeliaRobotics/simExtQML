#include "simPlusPlus/Plugin.h"
#include "simPlusPlus/Handle.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include "UI.h"
#include "SIM.h"
#include "Bridge.h"
#ifdef Qt5_Quick3D_FOUND
#include "Geometry.h"
#endif // Qt5_Quick3D_FOUND

#include <QBuffer>

template<> std::string sim::Handle<QQmlApplicationEngine>::tag() { return "QML.QQmlApplicationEngine"; }

class Plugin : public sim::Plugin
{
public:
    void onStart()
    {
        if(simGetBooleanParameter(sim_boolparam_headless) > 0)
            throw std::runtime_error("doesn't work in headless mode");

        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("QML User-Interface Plugin");
        setBuildDate(BUILD_DATE);

        Bridge::registerQmlType();
#ifdef Qt5_Quick3D_FOUND
        Geometry::registerQmlType();
#endif // Qt5_Quick3D_FOUND

        auto w = nullptr;//reinterpret_cast<QWidget*>(simGetMainWindow(1));
        ui = UI::getInstance(w);
    }

    void onFirstInstancePass(const sim::InstancePassFlags &flags)
    {
        sim = SIM::getInstance();
    }

    void onLastInstancePass()
    {
        SIM::destroyInstance();
        sim = nullptr;
    }

    void onEnd()
    {
        UI::destroyInstance();
        ui = nullptr;
    }

    void onInstanceSwitch(int sceneID)
    {
        if(sceneID == oldSceneID) return;

        for(auto engine : handles.findByScene(oldSceneID))
            sim->sendEventToQML(engine, "onInstanceSwitch", "false");

        for(auto engine : handles.findByScene(sceneID))
            sim->sendEventToQML(engine, "onInstanceSwitch", "true");

        oldSceneID = sceneID;
    }

    void onScriptStateDestroyed(int scriptID)
    {
        for(auto engine : handles.find(scriptID))
            sim->destroyEngine(handles.remove(engine));
    }

    void createEngine(createEngine_in *in, createEngine_out *out)
    {
        QStringList importPaths;
        importPaths << QString::fromStdString(sim::getStringParameter(sim_stringparam_application_path) + "/qml");
        importPaths << QString::fromStdString(sim::getStringParameter(sim_stringparam_scene_path));
        QQmlApplicationEngine *engine;
        sim->createEngine(&engine, importPaths);
        out->handle = handles.add(engine, in->_.scriptID);
        sim->setEngineHandle(engine, QString::fromStdString(out->handle));
    }

    void destroyEngine(destroyEngine_in *in, destroyEngine_out *out)
    {
        auto engine = handles.get(in->handle);
        sim->destroyEngine(handles.remove(engine));
    }

    QString getContextInfo(const SScriptCallBack &_)
    {
        return QString("%1:%2").arg(_.source).arg(_.line);
    }

    void load(load_in *in, load_out *out)
    {
        auto engine = handles.get(in->engineHandle);
        auto filename = QString::fromStdString(in->filename);
        sim->load(engine, filename, getContextInfo(in->_));
    }

    void loadData(loadData_in *in, loadData_out *out)
    {
        auto engine = handles.get(in->engineHandle);
        QByteArray data(in->data.c_str(), in->data.length());
        QString basepath(QString::fromStdString(in->basepath));
        sim->loadData(engine, data, basepath, getContextInfo(in->_));
    }

    void setEventHandlerRaw(setEventHandlerRaw_in *in, setEventHandlerRaw_out *out)
    {
        auto engine = handles.get(in->engineHandle);
        auto functionName = QString::fromStdString(in->functionName);
        sim->setEventHandler(engine, in->_.scriptID, functionName);
    }

    void sendEventRaw(sendEventRaw_in *in, sendEventRaw_out *out)
    {
        auto engine = handles.get(in->engineHandle);
        auto eventName = QString::fromStdString(in->eventName);
        QByteArray data(in->eventData.c_str(), in->eventData.length());
        sim->sendEventToQML(engine, eventName, data);
    }

    void imageDataURL(imageDataURL_in *in, imageDataURL_out *out)
    {
        auto imageFormat = imageFormats.find(in->format);
        if(imageFormat == imageFormats.end())
            throw sim::exception("invalid format: \"%s\"", in->format);
        auto imageDataFormat = imageDataFormats.find(in->data_format);
        if(imageDataFormat == imageDataFormats.end())
            throw sim::exception("invalid data format: \"%d\"", in->data_format);
        QByteArray data(in->data.c_str(), in->data.length());
        QImage im((const uchar *)data.data(), in->width, in->height, imageDataFormat->second);
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        im.save(&buffer, imageFormat->first.c_str());
        QByteArray data_b64 = ba.toBase64();
        out->dataURL = "data:";
        out->dataURL += imageFormat->second;
        out->dataURL += ";base64,";
        out->dataURL += std::string(data_b64.data());
    }

private:
    int oldSceneID = -1;
    UI *ui;
    SIM *sim;
    sim::Handles<QQmlApplicationEngine> handles;
    const std::map<std::string, std::string> imageFormats{
        {"PNG", "image/png"},
        {"BMP", "image/bmp"},
        {"JPG", "image/jpeg"}
    };
    const std::map<int, QImage::Format> imageDataFormats{
        {sim_qml_image_data_format_gray8,    QImage::Format_Grayscale8},
        {sim_qml_image_data_format_rgb888,   QImage::Format_RGB888},
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        {sim_qml_image_data_format_bgr888,   QImage::Format_BGR888},
#endif
        {sim_qml_image_data_format_rgbx8888, QImage::Format_RGBX8888},
        {sim_qml_image_data_format_rgb32,    QImage::Format_RGB32},
        {sim_qml_image_data_format_argb32,   QImage::Format_ARGB32}
    };
};

SIM_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
#include "stubsPlusPlus.cpp"
