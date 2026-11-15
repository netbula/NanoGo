#include "Review.h"

int Review::severity(double winLoss, double scoreLoss)
{
    if (scoreLoss >= 5.0 || winLoss >= 0.15) return 2;
    if (scoreLoss >= 2.0 || winLoss >= 0.07) return 1;
    return 0;
}

QList<MoveEval> Review::evaluate(const QList<GameNode *> &line)
{
    QList<MoveEval> out;
    for (int i = 1; i < line.size(); ++i) {
        MoveEval e;
        e.node = line[i];
        e.moveNo = i;
        e.color = line[i]->move.color;
        const Analysis &before = line[i - 1]->analysis;
        const Analysis &after = line[i]->analysis;
        if (e.color != Stone::Empty && before.valid && after.valid) {
            const double sign = e.color == Stone::Black ? 1.0 : -1.0;
            e.winLoss = qMax(0.0, sign * (before.winrateBlack - after.winrateBlack));
            e.scoreLoss = qMax(0.0, sign * (before.scoreLeadBlack - after.scoreLeadBlack));
            e.severity = severity(e.winLoss, e.scoreLoss);
            e.known = true;
        }
        out.append(e);
    }
    return out;
}
