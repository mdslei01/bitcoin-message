#include "messagecoder.h"
#include "bitcoinunits.h"

#include <boost/lexical_cast.hpp>

CMessageCoder::CMessageCoder() :
    terminator('.'), maxSymbolsPerCodeWord(7)
{
    // Contains the min and max value for each character
    characterRanges = createCharacterRanges();
}

QList<qint64> CMessageCoder::encode(const QString &message) const
{
    // The actual number of characters that can be encoded into 8 decimal places can vary based on the characters.
    // Therefore, encoding starts by trying to encode 7 characters and encodes 1 character less each time the
    // resulting encoded value exceeds 8 decimal places. This helps ensure maximum information density.

    // The part of the message encoded is paired with the encoded value
    QList<qint64> result;

    for (int i = 0, chunkLength = maxSymbolsPerCodeWord; i < message.length(); i += chunkLength, chunkLength = maxSymbolsPerCodeWord)
    {
        QString chunk = message.mid(i, chunkLength);
        qint64 encodedChunk;

        // Check for chunkLength > 0 to avoid any infinite loops caused by errors in the encoding algorithm
        while (chunkLength > 0 && !tryEncode(chunk + terminator, &encodedChunk))
        {
            // Reduce the size of the chunk by 1 and try again
            chunkLength--;
            chunk = message.mid(i, chunkLength);
        }

        // This should never happen but, if it does, there is a major error in the encoding algorithm
        if (chunkLength <= 0)
        {
            result.clear();
            return result;
        }

        result.append(encodedChunk);
    }

    return result;
}

QString CMessageCoder::decode(const QList<qint64> &encodedMessage) const
{
    QString message;
    qint64 chunk;

    // Decode each encoded chunk to form the entire message
    foreach (chunk, encodedMessage)
    {
        message.append(decodeChunk(BitcoinUnits::format(BitcoinUnits::BTC, chunk).toDouble()));
    }

    return message;
}

QMap<QChar, QPair<double, double> > CMessageCoder::createCharacterRanges() const
{
        QMap<QChar, QPair<double, double> > results;
        QVector<QPair<QChar, double> > frequencies = createFrequencyMap();

        double low = 0;
        QPair<QChar, double> i;

        foreach (i, frequencies)
        {
            // The high value of this character is the cumulative sum of this character's and
            // previous character's relative frequencies
            double high = i.second + low;

            results[i.first] = QPair<double, double>(low, high);

            // The low value of the next character is the high value of this character
            low = high;
        }

        return results;
}

QVector<QPair<QChar, double> > CMessageCoder::createFrequencyMap() const
{
    QVector<QPair<QChar, double> > frequencies;

    // The relative frequencies of all the valid characters
    frequencies.append(QPair<QChar, double>('a', 0.0609));
    frequencies.append(QPair<QChar, double>('b', 0.0105));
    frequencies.append(QPair<QChar, double>('c', 0.0284));
    frequencies.append(QPair<QChar, double>('d', 0.0292));
    frequencies.append(QPair<QChar, double>('e', 0.1136));
    frequencies.append(QPair<QChar, double>('f', 0.0179));
    frequencies.append(QPair<QChar, double>('g', 0.0138));
    frequencies.append(QPair<QChar, double>('h', 0.0341));
    frequencies.append(QPair<QChar, double>('i', 0.0544));
    frequencies.append(QPair<QChar, double>('j', 0.0024));
    frequencies.append(QPair<QChar, double>('k', 0.0041));
    frequencies.append(QPair<QChar, double>('l', 0.0292));
    frequencies.append(QPair<QChar, double>('m', 0.0276));
    frequencies.append(QPair<QChar, double>('n', 0.0544));
    frequencies.append(QPair<QChar, double>('o', 0.0600));
    frequencies.append(QPair<QChar, double>('p', 0.0195));
    frequencies.append(QPair<QChar, double>('q', 0.0024));
    frequencies.append(QPair<QChar, double>('r', 0.0495));
    frequencies.append(QPair<QChar, double>('s', 0.0568));
    frequencies.append(QPair<QChar, double>('t', 0.0803));
    frequencies.append(QPair<QChar, double>('u', 0.0243));
    frequencies.append(QPair<QChar, double>('v', 0.0097));
    frequencies.append(QPair<QChar, double>('w', 0.0138));
    frequencies.append(QPair<QChar, double>('x', 0.0024));
    frequencies.append(QPair<QChar, double>('y', 0.0130));
    frequencies.append(QPair<QChar, double>('z', 0.0003));
    frequencies.append(QPair<QChar, double>(' ', 0.1217));
    frequencies.append(QPair<QChar, double>(terminator, 0.0658));

    return frequencies;
}

bool CMessageCoder::tryEncode(const QString &text, qint64 *result) const
{
    // Arithmetic Coding algorithm
    long double low = 0.0, high = 1.0;

    for (int i = 0; i < text.length(); i++)
    {
        long double range = high - low;
        long double previousLow = low;

        low = previousLow + range * characterRanges[text[i]].first;
        high = previousLow + range * characterRanges[text[i]].second;
    }

    // Note: Currently, low can never be 0 and high can never be 1 nor can either be >= 1.
    // Low can never be 0 because that would require a string of only a's. All a's is not possible
    // because the terminator character is always appended to the message.
    // High can never be 1 because that would require a string of only .'s. All .'s is not possible
    // because the user cannot enter in .'s nor can the user enter in a blank message.
    // If low could be 0 or high could be 1, the following code would not work.

    // Since the Arithmetic Coding algorithm results in a valid range for the encoded message, we
    // can select a value that has the fewest number of digits. This is done by keeping all the
    // digits that are the same between the low and high values until we reach a digit that is
    // different. At this point, we can likely take the high value's digit. However, if doing so
    // would make the code word equal to the high value then we must, instead, find the value with
    // the fewest digits that is greater than or equal to the low value.
    QString lowString = QString::fromStdString(boost::lexical_cast<std::string, long double>(low));
    QString highString = QString::fromStdString(boost::lexical_cast<std::string, long double>(high));
    QString output = "0.";

    for (int i = 2; i < lowString.length() && i < highString.length(); i++)
    {
        if (lowString[i] == highString[i])
        {
            output.append(lowString[i]);
        }
        else if ((output + highString[i]).toDouble() == high)
        {
            // The code word cannot be equal to the high value. Therefore, we must find the
            // shortest value that is >= the low value

            output.append(lowString[i]);

            // Find the first digit that is not a 9, increment it, and that is the final code word.
            // If the low value only contains 9's beyond this point then the code word will equal
            // the low value.
            for (i++; i < lowString.length(); i++)
            {
                int digit = QString(lowString[i]).toInt();

                if (digit == 9)
                {
                    output.append(lowString[i]);
                }
                else
                {
                    output.append(QString::number(digit + 1));
                    break;
                }
            }

            break;
        }
        else
        {
            output.append(highString[i]);
            break;
        }
    }

    // Use the existing bitcoin parsing function to validate the result
    return BitcoinUnits::parse(BitcoinUnits::BTC, output, result);
}

QString CMessageCoder::decodeChunk(long double value) const
{
    // Arithmetic Decoding algorithm
    QString decodedMessage;
    QChar symbol;

    while (true)
    {
        long double low = 0.0, high = 0.0;

        // Search for the range that the value falls within to
        // determine the next character
        foreach (symbol, characterRanges.keys())
        {
            low = characterRanges[symbol].first;
            high = characterRanges[symbol].second;

            if (value >= low && value < high)
            {
                break;
            }
        }

        if (symbol == terminator)
        {
            break;
        }

        decodedMessage.append(symbol);

        // Stop if we've decoded more than the max number of symbols per code word
        // (Prevents infinite loops if attempting to decode an invalid code word).
        if (decodedMessage.length() > maxSymbolsPerCodeWord)
        {
            decodedMessage.clear();
            break;
        }

        value = (value - low) / (high - low);
    }

    return decodedMessage;
}
