#ifndef MESSAGECODER_H
#define MESSAGECODER_H

#include <QMap>
#include <QChar>
#include <QPair>
#include <QVector>
#include <QString>
#include <QList>

class CMessageCoder
{    
public:
    CMessageCoder();

    QList<qint64> encode(const QString &message) const;
    QString decode(const QList<qint64> &encodedMessage) const;

private:
    const QChar terminator;
    const int maxSymbolsPerCodeWord;
    QMap<QChar, QPair<double, double> > characterRanges;

    QMap<QChar, QPair<double, double> > createCharacterRanges() const;
    QVector<QPair<QChar, double> > createFrequencyMap() const;
    bool tryEncode(const QString &text, qint64 *result) const;
    QString decodeChunk(long double value) const;
};

#endif // MESSAGECODER_H
