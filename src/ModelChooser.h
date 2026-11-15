#pragma once
#include "ModelUpdater.h"
#include <QDialog>

class QTableWidget;
class QCheckBox;

// 让用户自己挑模型：官方列表（等级分、大小、评测局数、日期）+ 本机已有的模型 + 任意文件
class ModelChooser : public QDialog {
    Q_OBJECT
public:
    ModelChooser(const QList<NetworkInfo> &official, const QString &currentPath, QWidget *parent = nullptr);

    // 选中官方模型时 network 有效；选中本机文件时 localPath 有效
    NetworkInfo network() const { return m_network; }
    QString localPath() const { return m_localPath; }

private:
    void fill();
    void acceptRow();

    QList<NetworkInfo> m_official;
    QString m_current;
    QTableWidget *m_table;
    QCheckBox *m_onlyRated;
    NetworkInfo m_network;
    QString m_localPath;
};
