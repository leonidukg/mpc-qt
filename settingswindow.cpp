#include "settingswindow.h"
#include "ui_settingswindow.h"
#include <QDebug>
#include <QDialogButtonBox>
#include <QHash>

#define SCALAR_SCALARS \
    "bilinear", "bicubic_fast", "oversample", "spline16", "spline36",\
    "spline64", "sinc", "lanczos", "gingseng", "jinc", "ewa_lanczos",\
    "ewa_hanning", "ewa_gingseng", "ewa_lanczossharp", "ewa_lanczossoft",\
    "hassnsoft",  "bicubic", "bcspline", "catmull_rom", "mitchell",\
    "robidoux", "robidouxsharp", "ewa_robidoux", "ewa_robidouxsharp",\
    "box", "nearest", "triangle", "gaussian"

#define SCALAR_WINDOWS \
    "box", "triable", "bartlett", "hanning", "hamming", "quadric", "welch",\
    "kaiser", "blackman", "gaussian", "sinc", "jinc", "sphinx"

#define TIME_SCALARS \
    "oversample", "spline16", "spline36", "spline64", "sinc", "lanczos",\
    "gingseng", "catmull_rom", "mitchell", "robidoux", "robidouxsharp",\
    "box", "nearest", "triangle", "gaussian"


QHash<QString, QStringList> SettingMap::indexedValueToText = {
    {"videoFramebuffer", {"rgb8-rgba", "rgb10-rgb10_a2", "rgba12-rgba12", "rgb16-rgba16", "rgb16f-rgba16f", "rgb32f-rgba32f"}},
    {"videoAlphaMode", {"blend", "yes", "no"}},
    {"ditherType", {"fruit", "ordered", "no"}},
    {"scaleScalar", {SCALAR_SCALARS}},
    {"scaleWindow", {SCALAR_WINDOWS}},
    {"dscaleScalar", {SCALAR_SCALARS}},
    {"dscaleWindow", {SCALAR_WINDOWS}},
    {"cscaleScalar", {SCALAR_SCALARS}},
    {"cscaleWindow", {SCALAR_WINDOWS}},
    {"tscaleScalar", {TIME_SCALARS}},
    {"tscaleWindow", {SCALAR_WINDOWS}},
    {"prescalarMethod", {"none", "superxbr", "needi3"}},
    {"nnedi3Neurons", {"16", "32", "64", "128"}},
    {"nnedi3Window", {"8x4", "8x6"}},
    {"nnedi3Upload", {"ubo", "shader"}},
    {"ccTargetPrim", {"auto", "bt.601-525", "bt.601-625", "bt.709", "bt.2020", "bt.470m", "apple", "adobe", "prophoto", "cie1931"}},
    {"ccTargetTRC", {"auto", "by.1886", "srgb", "linear", "gamma1.8", "gamma2.2", "gamma2.8", "prophoto"}},
    {"audioRenderer", {"pulse", "alsa", "oss", "null"}},
    {"framedroppingMode", {"no", "vo", "decoder", "decoder+vo"}},
    {"framedroppingDecoderMode", {"none", "default", "nonref", "bidir", "nonkey", "all"}},
    {"syncMode", {"audio", "display-resample", "display-resample-vdrop", "display-resample-desync", "display-adrop", "display-vdrop"}},
    {"subtitlePlacementX", {"left", "center", "right"}},
    {"subtitlePlacementY", {"top", "center", "bottom"}},
    {"subtitlesAssOverride", {"no", "yes", "force", "signfs"}},
    {"subtitleAlignment", { "top-center", "top-right", "center-right", "bottom-right", "bottom-center", "bottom-left", "center-left", "top-left", "center-center" }}
};

QMap<QString, QString> Setting::classToProperty = {
    { "QCheckBox", "checked" },
    { "QRadioButton", "checked" },
    { "QLineEdit", "text" },
    { "QSpinBox", "value" },
    { "QDoubleSpinBox", "value" },
    { "QComboBox", "currentIndex" },
    { "QListWidget", "currentRow" },
    { "QFontComboBox", "currentText" },
    { "QScrollBar", "value" }
};

void Setting::sendToControl()
{
    QString property = classToProperty[widget->metaObject()->className()];
    widget->setProperty(property.toUtf8().data(), value);
}

void Setting::fetchFromControl()
{
    QString property = classToProperty[widget->metaObject()->className()];
    value = widget->property(property.toUtf8().data());
}

QVariantMap SettingMap::toVMap()
{
    QVariantMap m;
    foreach (Setting s, *this)
        m.insert(s.name, s.value);
    return m;
}

void SettingMap::fromVMap(const QVariantMap &m)
{
    // read settings from variant, but only try to insert if they are already
    // there. (don't accept nonsense.)  To use this function properly, it is
    // necessary to call SettingsWindow::generateSettingsMap first.
    QMapIterator<QString, QVariant> i(m);
    while (i.hasNext()) {
        i.next();
        if (!this->contains(i.key()))
            continue;
        Setting s = this->value(i.key());
        this->insert(s.name, {s.name, s.widget, i.value()});
    }
}


SettingsWindow::SettingsWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsWindow)
{
    ui->setupUi(this);

    defaultSettings = generateSettingMap();
    acceptedSettings = defaultSettings;

    ui->pageStack->setCurrentIndex(0);
    ui->videoTabs->setCurrentIndex(0);
    ui->scalingTabs->setCurrentIndex(0);
    ui->prescalarStack->setCurrentIndex(0);
    ui->audioRendererStack->setCurrentIndex(0);

    // Expand every item on pageTree
    QList<QTreeWidgetItem*> stack;
    stack.append(ui->pageTree->invisibleRootItem());
    while (!stack.isEmpty()) {
        QTreeWidgetItem* item = stack.takeFirst();
        item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i)
            stack.push_front(item->child(i));
    }
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

void SettingsWindow::updateAcceptedSettings() {
    acceptedSettings = generateSettingMap();
}

SettingMap SettingsWindow::generateSettingMap()
{
    SettingMap settingMap;


    // The idea here is to discover all the widgets in the ui and only inspect
    // the widgets which we desire to know about.
    QObjectList toParse;
    toParse.append(this);
    while (!toParse.empty()) {
        QObject *item = toParse.takeFirst();
        if (QStringList({"QCheckBox", "QRadioButton", "QLineEdit", "QSpinBox",
                        "QDoubleSpinBox", "QComboBox", "QListWidget", "QFontComboBox",
                        "QScrollBar"}).contains(item->metaObject()->className())
            && !item->objectName().isEmpty()
            && item->objectName() != "qt_spinbox_lineedit") {
            QString name = item->objectName();
            QString className = item->metaObject()->className();
            QString property = Setting::classToProperty.value(className, QString());
            QVariant value = item->property(property.toUtf8().data());
            settingMap.insert(name, {name, qobject_cast<QWidget*>(item), value});
            continue;
        }
        QObjectList children = item->children();
        foreach(QObject *child, children) {
            if (child->inherits("QWidget") || child->inherits("QLayout"))
            toParse.append(child);
        }
    };
    return settingMap;
}

void SettingsWindow::takeSettings(QVariantMap payload)
{
    acceptedSettings.fromVMap(payload);
    for (Setting &s : acceptedSettings) {
        s.sendToControl();
    }
}


#define WIDGET_LOOKUP(widget) \
    acceptedSettings[widget->objectName()].value

#define OFFSET_LOOKUP(source, widget) \
    source[widget->objectName()].value.toInt()

#define WIDGET_TO_TEXT(widget) \
    SettingMap::indexedValueToText[widget->objectName()].value(OFFSET_LOOKUP(acceptedSettings,widget), \
        SettingMap::indexedValueToText[widget->objectName()].value(OFFSET_LOOKUP(defaultSettings,widget)))

void SettingsWindow::sendSignals()
{
    QMap<QString,QString> params;
    QStringList cmdline;

    params["fbo-format"] = WIDGET_TO_TEXT(ui->videoFramebuffer).split('-').value(WIDGET_LOOKUP(ui->videoUseAlpha).toBool());
    params["alpha"] = WIDGET_TO_TEXT(ui->videoUseAlpha);
    params["sharpen"] = WIDGET_LOOKUP(ui->videoSharpen).toString();

    if (WIDGET_LOOKUP(ui->ditherDithering).toBool()) {
        params["dither-depth"] = WIDGET_TO_TEXT(ui->ditherDepth);
        params["dither"] = WIDGET_TO_TEXT(ui->ditherType);
        params["dither-size-fruit"] = WIDGET_LOOKUP(ui->ditherFruitSize).toString();
    }
    if (WIDGET_LOOKUP(ui->ditherTemporal).toBool()) {
        params["temporal-dither"] = QString();
        params["temporal-dither-period"] = WIDGET_LOOKUP(ui->ditherTemporalPeriod).toString();
    }
    // WIDGET_LOOKUP().toString();
    // WIDGET_LOOKUP().toBool()
    if (WIDGET_LOOKUP(ui->scalingCorrectDownscaling).toBool())
        params["correct-downscaling"] = QString();
    if (WIDGET_LOOKUP(ui->scalingInLinearLight).toBool())
        params["linear-scaling"] = QString();
    if (WIDGET_LOOKUP(ui->scalingTemporalInterpolation).toBool())
        params["interpolation"] = QString();
    if (WIDGET_LOOKUP(ui->scalingBlendSubtitles).toBool())
        params["blend-subtitles"] = QString();
    if (WIDGET_LOOKUP(ui->scalingSigmoidizedUpscaling).toBool()) {
        params["sigmoid-upscaling"] = QString();
        params["sigmoid-center"] = WIDGET_LOOKUP(ui->sigmoidizedCenter).toString();
        params["sigmoid-slope"] = WIDGET_LOOKUP(ui->sigmoidizedSlope).toString();
    }

    params["scale"] = WIDGET_TO_TEXT(ui->scaleScalar);
    if (WIDGET_LOOKUP(ui->scaleParam1Set).toBool())
        params["scale-param1"] = WIDGET_LOOKUP(ui->scaleParam1Value).toString();
    if (WIDGET_LOOKUP(ui->scaleParam2Set).toBool())
        params["scale-param2"] = WIDGET_LOOKUP(ui->scaleParam2Value).toString();
    if (WIDGET_LOOKUP(ui->scaleRadiusSet).toBool())
        params["scale-radius"] = WIDGET_LOOKUP(ui->scaleRadiusValue).toString();
    if (WIDGET_LOOKUP(ui->scaleAntiRingSet).toBool())
        params["scale-antiring"] = WIDGET_LOOKUP(ui->scaleAntiRingValue).toString();
    if (WIDGET_LOOKUP(ui->scaleBlurSet).toBool())
        params["scale-blur"] = WIDGET_LOOKUP(ui->scaleBlurValue).toString();
    if (WIDGET_LOOKUP(ui->scaleWindowParamSet).toBool())
        params["scale-wparam"] = WIDGET_LOOKUP(ui->scaleWindowParamValue).toString();
    if (WIDGET_LOOKUP(ui->scaleWindowSet).toBool())
        params["scale-window"] = WIDGET_TO_TEXT(ui->scaleWindowValue);
    if (WIDGET_LOOKUP(ui->scaleClamp).toBool())
        params["scale-clamp"] = QString();

    if (WIDGET_LOOKUP(ui->dscaleScalar).toInt())
        params["dscale"] = WIDGET_TO_TEXT(ui->dscaleScalar);
    if (WIDGET_LOOKUP(ui->dscaleParam1Set).toBool())
        params["dscale-param1"] = WIDGET_LOOKUP(ui->scaleParam1Value).toString();
    if (WIDGET_LOOKUP(ui->dscaleParam2Set).toBool())
        params["dscale-param2"] = WIDGET_LOOKUP(ui->scaleParam2Value).toString();
    if (WIDGET_LOOKUP(ui->dscaleRadiusSet).toBool())
        params["dscale-radius"] = WIDGET_LOOKUP(ui->scaleRadiusValue).toString();
    if (WIDGET_LOOKUP(ui->dscaleAntiRingSet).toBool())
        params["dscale-antiring"] = WIDGET_LOOKUP(ui->scaleAntiRingValue).toString();
    if (WIDGET_LOOKUP(ui->dscaleBlurSet).toBool())
        params["dscale-blur"] = WIDGET_LOOKUP(ui->scaleBlurValue).toString();
    if (WIDGET_LOOKUP(ui->dscaleWindowParamSet).toBool())
        params["dscale-wparam"] = WIDGET_LOOKUP(ui->scaleWindowParamValue).toString();
    if (WIDGET_LOOKUP(ui->dscaleWindowSet).toBool())
        params["dscale-window"] = WIDGET_TO_TEXT(ui->scaleWindowValue);
    if (WIDGET_LOOKUP(ui->dscaleClamp).toBool())
        params["dscale-clamp"] = QString();

    params["cscale"] = WIDGET_TO_TEXT(ui->cscaleScalar);
    if (WIDGET_LOOKUP(ui->cscaleParam1Set).toBool())
        params["ccale-param1"] = WIDGET_LOOKUP(ui->cscaleParam1Value).toString();
    if (WIDGET_LOOKUP(ui->cscaleParam2Set).toBool())
        params["cscale-param2"] = WIDGET_LOOKUP(ui->cscaleParam2Value).toString();
    if (WIDGET_LOOKUP(ui->cscaleRadiusSet).toBool())
        params["cscale-radius"] = WIDGET_LOOKUP(ui->cscaleRadiusValue).toString();
    if (WIDGET_LOOKUP(ui->cscaleAntiRingSet).toBool())
        params["cscale-antiring"] = WIDGET_LOOKUP(ui->cscaleAntiRingValue).toString();
    if (WIDGET_LOOKUP(ui->cscaleBlurSet).toBool())
        params["cscale-blur"] = WIDGET_LOOKUP(ui->cscaleBlurValue).toString();
    if (WIDGET_LOOKUP(ui->cscaleWindowParamSet).toBool())
        params["cscale-wparam"] = WIDGET_LOOKUP(ui->cscaleWindowParamValue).toString();
    if (WIDGET_LOOKUP(ui->cscaleWindowSet).toBool())
        params["cscale-window"] = WIDGET_TO_TEXT(ui->cscaleWindowValue);
    if (WIDGET_LOOKUP(ui->cscaleClamp).toBool())
        params["cscale-clamp"] = QString();

    params["tscale"] = WIDGET_TO_TEXT(ui->tscaleScalar);
    if (WIDGET_LOOKUP(ui->tscaleParam1Set).toBool())
        params["tscale-param1"] = WIDGET_LOOKUP(ui->tscaleParam1Value).toString();
    if (WIDGET_LOOKUP(ui->tscaleParam2Set).toBool())
        params["tscale-param2"] = WIDGET_LOOKUP(ui->tscaleParam2Value).toString();
    if (WIDGET_LOOKUP(ui->tscaleRadiusSet).toBool())
        params["tscale-radius"] = WIDGET_LOOKUP(ui->tscaleRadiusValue).toString();
    if (WIDGET_LOOKUP(ui->tscaleAntiRingSet).toBool())
        params["tscale-antiring"] = WIDGET_LOOKUP(ui->tscaleAntiRingValue).toString();
    if (WIDGET_LOOKUP(ui->tscaleBlurSet).toBool())
        params["tscale-blur"] = WIDGET_LOOKUP(ui->tscaleBlurValue).toString();
    if (WIDGET_LOOKUP(ui->tscaleWindowParamSet).toBool())
        params["tscale-wparam"] = WIDGET_LOOKUP(ui->tscaleWindowParamValue).toString();
    if (WIDGET_LOOKUP(ui->tscaleWindowSet).toBool())
        params["tscale-window"] = WIDGET_TO_TEXT(ui->tscaleWindowValue);
    if (WIDGET_LOOKUP(ui->tscaleClamp).toBool())
        params["tscale-clamp"] = QString();

    if (WIDGET_LOOKUP(ui->debandEnabled).toBool()) {
        params["deband"] = QString();
        params["deband-iterations"] = WIDGET_LOOKUP(ui->debandIterations).toString();
        params["deband-threshold"] = WIDGET_LOOKUP(ui->debandThreshold).toString();
        params["deband-range"] = WIDGET_LOOKUP(ui->debandRange).toString();
        params["deband-grain"] = WIDGET_LOOKUP(ui->debandGrain).toString();
    }

    QMapIterator<QString,QString> i(params);
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) {
            cmdline.append(QString("%1=%2").arg(i.key(),i.value()));
        } else {
            cmdline.append(i.key());
        }
    }
    voCommandLine(WIDGET_LOOKUP(ui->videoDumbMode).toBool()
                  ? "dumb-mode" : cmdline.join(':'));

    framedropMode(WIDGET_TO_TEXT(ui->framedroppingMode));
    decoderDropMode(WIDGET_TO_TEXT(ui->framedroppingDecoderMode));
    displaySyncMode(WIDGET_TO_TEXT(ui->syncMode));
    audioDropSize(WIDGET_LOOKUP(ui->syncAudioDropSize).toDouble());
    maximumAudioChange(WIDGET_LOOKUP(ui->syncMaxAudioChange).toDouble());
    maximumVideoChange(WIDGET_LOOKUP(ui->syncMaxVideoChange).toDouble());
    subsAreGray(WIDGET_LOOKUP(ui->subtitlesForceGrayscale).toDouble());
}

void SettingsWindow::on_pageTree_itemSelectionChanged()
{
    QModelIndex modelIndex = ui->pageTree->currentIndex();
    if (!modelIndex.isValid())
        return;

    static int parentIndex[] = { 0, 4, 9, 12, 13 };
    int index = 0;
    if (!modelIndex.parent().isValid())
        index = parentIndex[modelIndex.row()];
    else
        index = parentIndex[modelIndex.parent().row()] + modelIndex.row() + 1;
    ui->pageStack->setCurrentIndex(index);
    ui->pageLabel->setText(QString("<big><b>%1</b></big>").
                           arg(modelIndex.data().toString()));
}

void SettingsWindow::on_buttonBox_clicked(QAbstractButton *button)
{
    QDialogButtonBox::ButtonRole buttonRole;
    buttonRole = ui->buttonBox->buttonRole(button);
    if (buttonRole == QDialogButtonBox::ApplyRole ||
            buttonRole == QDialogButtonBox::AcceptRole) {\
        updateAcceptedSettings();
        emit settingsData(acceptedSettings.toVMap());
        sendSignals();
    }
    if (buttonRole == QDialogButtonBox::AcceptRole ||
            buttonRole == QDialogButtonBox::RejectRole)
        close();
}

void SettingsWindow::on_prescalarMethod_currentIndexChanged(int index)
{
    ui->prescalarStack->setCurrentIndex(index);
}

void SettingsWindow::on_audioRenderer_currentIndexChanged(int index)
{
    ui->audioRendererStack->setCurrentIndex(index);
}

void SettingsWindow::on_videoDumbMode_toggled(bool checked)
{
    ui->videoTabs->setEnabled(!checked);
}

