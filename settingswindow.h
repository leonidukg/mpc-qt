#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QAbstractButton>
#include <QVariantMap>

class Setting {
public:
    Setting() : name(), widget(NULL), value() {}
    Setting(const Setting &s) : name(s.name), widget(s.widget), value(s.value) {}
    Setting(QString name, QWidget *widget, QVariant value) : name(name), widget(widget), value(value) {}

    ~Setting() {}
    QString name;
    QWidget *widget;
    QVariant value;
};


struct Settings {
public:
    explicit Settings() {}
    static QHash<QString, QStringList> indexedValueToText;
    QVariantMap toVMap();
    void fromVMap(const QVariantMap &m);
};

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = 0);
    ~SettingsWindow();

private:
    void updateAcceptedSettings();
    void generateSettingMap();

signals:
    void settingsData(const Settings &s);

    void voCommandLine(const QString &s);
    void framedropMode(const QString &s);
    void decoderDropMode(const QString &s);
    void displaySyncMode(const QString &s);
    void audioDropSize(double d);
    void maximumAudioChange(double d);
    void maximumVideoChange(double d);
    void subsAreGray(bool flag);

public slots:
    void takeSettings(const Settings &s);
    void sendSignals();

private slots:
    void on_pageTree_itemSelectionChanged();

    void on_buttonBox_clicked(QAbstractButton *button);

    void on_prescalarMethod_currentIndexChanged(int index);

    void on_audioRenderer_currentIndexChanged(int index);

    void on_videoDumbMode_toggled(bool checked);

private:
    Ui::SettingsWindow *ui;
    Settings acceptedSettings;
    QHash<QString, Setting> settingMap;
};

#endif // SETTINGSWINDOW_H
