#pragma once
#include "AppSettings.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(const AppSettings &s, const QStringList &devices, QWidget *parent = nullptr);
    AppSettings result() const;

private:
    AppSettings m_s;
    QLineEdit *m_katago, *m_model;
    QComboBox *m_gpu, *m_rules;
    QSpinBox *m_threads, *m_visits;
    QDoubleSpinBox *m_komi;
};
