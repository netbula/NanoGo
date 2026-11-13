#pragma once
#include "GameTree.h"
#include <QString>

namespace Sgf {
// 读取 SGF 文本到 tree。失败返回 false，并在 error 中给出中文原因。
bool load(const QString &text, GameTree &tree, QString *error);
QString save(const GameTree &tree);
// 自动识别 UTF-8 / GB18030 编码
QString decode(const QByteArray &bytes);
}
