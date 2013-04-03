#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QQueue>
#include <QSet>

#include "walletmodel.h"
#include "clientmodel.h"
#include "messagecoder.h"
#include "serialize.h"
#include "sync.h"

QT_BEGIN_NAMESPACE
class QDate;
QT_END_NAMESPACE

class MessageModel : public QObject
{
    Q_OBJECT
public:
    explicit MessageModel(WalletModel *walletModel, ClientModel *clientModel, QObject *parent = 0);

    class EncodedMessage
    {
        friend class MessageModel;

    public:
        EncodedMessage() { }

        QString getMessage() const { return QString::fromStdString(message); }
        QString getAddress() const { return QString::fromStdString(address); }
        qint64 getAmount() const { return amount; }
        bool isSendToSelf() const { return sendToSelf; }

        IMPLEMENT_SERIALIZE
        (
            READWRITE(message);
            READWRITE(address);
            READWRITE(amount);
            READWRITE(sendToSelf);
            READWRITE(lastTransaction);
            READWRITE(transactionCount);

            EncodedMessage *message = const_cast<EncodedMessage *>(this);
            int numRecipients = allRecipients.length();

            READWRITE(numRecipients);

            if (fWrite)
            {
                for (int i = 0; i < numRecipients; i++)
                {
                    READWRITE(allRecipients[i]);
                }
            }
            else
            {
                for (int i = 0; i < numRecipients; i++)
                {
                    SendCoinsRecipient recipient;
                    READWRITE(recipient);
                    message->allRecipients.enqueue(recipient);
                }
            }
        )

    protected:
        QQueue<SendCoinsRecipient> allRecipients;
        std::string message;
        std::string address;
        qint64 amount;
        bool sendToSelf;
        std::string lastTransaction;
        int transactionCount;
    };

    IMPLEMENT_SERIALIZE
    (
        MessageModel *model = const_cast<MessageModel *>(this);
        int numMessages = pendingMessages.length();

        READWRITE(numMessages);

        for (int i = 0; i < numMessages; i++)
        {
            if (fWrite)
            {
                READWRITE(*pendingMessages[i]);
            }
            else
            {
                EncodedMessage message;

                READWRITE(message);

                model->pendingMessages.append(new EncodedMessage(message));
            }
        }
    )

    bool searchForMessage(const QString address, const QDate startDate, const QDate endDate, QString *message) const;
    bool initializeMessage(const QString messageText, const QString address, EncodedMessage &message) const;
    bool sendMessage(EncodedMessage message);
    QList<QPair<int, int> > getMessageProgress() const;
    void closing();

signals:
    void messageStatusChanged(QList<QPair<int, int> > messageProgress);

private:
    WalletModel *walletModel;
    ClientModel *clientModel;
    CMessageCoder coder;
    QList<EncodedMessage *> pendingMessages;
    CCriticalSection cs_message;

    bool sendNextChunk(EncodedMessage *message);
    const char* getPathToMessageFile() const;
    bool loadPreviousMessages();
    bool saveCurrentMessages();

private slots:
    void numBlocksChanged(int count, int countOfPeers);
};

#endif // MESSAGEMODEL_H
