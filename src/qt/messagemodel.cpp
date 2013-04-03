#include "messagemodel.h"
#include "messagecoder.h"
#include "main.h"
#include "base58.h"
#include "ui_interface.h"
#include "addresstablemodel.h"
#include "db.h"
#include "util.h"

#include <QDateTime>

MessageModel::MessageModel(WalletModel *walletModel, ClientModel *clientModel, QObject *parent) :
    QObject(parent), walletModel(walletModel), clientModel(clientModel)
{
    if (!loadPreviousMessages())
    {
        uiInterface.ThreadSafeMessageBox(
                    "Unable to load previous messages. Any parts of a message that have not been sent will be lost.",
                    "Bitcoin: Unable to Load Messages",
                    CClientUIInterface::MODAL);
    }

    connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(numBlocksChanged(int,int)));
}

QList<QPair<int, int> > MessageModel::getMessageProgress() const
{
    QList<QPair<int, int> > progress;

    BOOST_FOREACH (EncodedMessage *message, pendingMessages)
    {
        progress.append(QPair<int, int>(message->transactionCount - message->allRecipients.length(), message->transactionCount));
    }

    return progress;
}

void MessageModel::closing()
{
    if (!saveCurrentMessages())
    {
        uiInterface.ThreadSafeMessageBox(
                    "Unable to save messages. Any parts of a message that have not been sent will be lost.",
                    "Bitcoin: Unable to Save Messages",
                    CClientUIInterface::MODAL);
    }
}

bool MessageModel::searchForMessage(const QString address, const QDate startDate, const QDate endDate, QString *message) const
{
    QList<qint64> encodedChunks;
    CBitcoinAddress searchForAddress(address.toStdString());

    // Begin at the genesis block
    for (CBlockIndex* pindex = pindexGenesisBlock; pindex; pindex = pindex->pnext)
    {
        QDate blockDate = QDateTime::fromTime_t(pindex->GetBlockTime()).date();

        // If we haven't gotten to blocks after the start date, move to next block
        if (blockDate < startDate)
        {
            continue;
        }

        // If we have gotten to blocks after the end date, stop searching
        if (blockDate > endDate)
        {
            break;
        }

        CBlock block;

        block.ReadFromDisk(pindex);

        // For each transaction
        BOOST_FOREACH (CTransaction& tx, block.vtx)
        {
            // For each output of the transaction
            BOOST_FOREACH (CTxOut& out, tx.vout)
            {
                CTxDestination destination;

                if (!ExtractDestination(out.scriptPubKey, destination))
                {
                    continue;
                }

                CBitcoinAddress address(destination);

                if (searchForAddress == address)
                {
                    encodedChunks.append(out.nValue);
                }
            }
        }
    }

    // If no payments to the address were found
    if (encodedChunks.length() == 0)
    {
        return false;
    }

    *message = coder.decode(encodedChunks);

    return true;
}

bool MessageModel::initializeMessage(const QString messageText, const QString address, EncodedMessage &message) const
{
    QList<qint64> codeWords = coder.encode(messageText);

    // Should not happen unless there is a bug in the encoding algorithm
    if (codeWords.length() == 0)
    {
        return false;
    }

    message.message = messageText.toStdString();
    message.address = address.toStdString();
    message.sendToSelf = walletModel->isAddressMine(address);
    message.transactionCount = codeWords.length();
    message.amount = 0;

    // Create the recipients and calculate the sum
    BOOST_FOREACH (qint64 &amount, codeWords)
    {
        SendCoinsRecipient recipient;

        recipient.address = address;
        recipient.label = walletModel->getAddressTableModel()->labelForAddress(address);
        recipient.amount = amount;

        message.allRecipients.enqueue(recipient);
        message.amount += amount;
    }

    return true;
}

bool MessageModel::sendMessage(EncodedMessage message)
{
    bool result;
    EncodedMessage *permanentMessage = new EncodedMessage(message);

    pendingMessages.append(permanentMessage);
    result = sendNextChunk(permanentMessage);

    // If sending resulted in an error, delete the message
    if (!result)
    {
        pendingMessages.removeOne(permanentMessage);
        delete permanentMessage;
    }

    emit messageStatusChanged(getMessageProgress());

    return result;
}

bool MessageModel::sendNextChunk(EncodedMessage *message)
{
    SendCoinsRecipient nextRecipient = message->allRecipients.dequeue();
    QList<SendCoinsRecipient> sendList;

    sendList.append(nextRecipient);

    WalletModel::SendCoinsReturn result = walletModel->sendCoins(sendList, false);

    // If an error occurred with the transaction, notify the user and remove the remaining message
    if (result.status == WalletModel::TransactionCreationFailed || result.status == WalletModel::TransactionCommitFailed)
    {
        uiInterface.ThreadSafeMessageBox(
                    "An error occurred while processing a transaction for a message. The remaining portions of the message cannot be sent.",
                    "Send Message Error",
                    CClientUIInterface::MODAL);

        return false;
    }

    // If the transaction was sent successfully
    if (result.status == WalletModel::OK)
    {
        // Update the last transaction so we can determine when it has been included in a block
        message->lastTransaction = result.hex.toStdString();
    }
    else
    {
        // The other reason for failure is not enough bitcoins, simply continue to try sending when
        // the number of blocks change until it is successful
        message->allRecipients.push_front(nextRecipient);
    }

    return true;
}

void MessageModel::numBlocksChanged(int count, int countOfPeers)
{
    // Don't update the messages until we have the entire block chain. Deadlocks can occur otherwise.
    // Also, if there are no pending messages, there's nothing to do.
    if (count != countOfPeers || pendingMessages.empty())
    {
        return;
    }

    CTxDB transactionDB;
    QList<EncodedMessage *> messagesToRemove;

    BOOST_FOREACH (EncodedMessage *message, pendingMessages)
    {
        bool result = true;
        QString hex = QString::fromStdString(message->lastTransaction);

        // Ensure the last message chunk has been included in a block before sending the next
        if (!hex.isEmpty() && transactionDB.ContainsTx((uint256)hex.toStdString()))
        {
            result = sendNextChunk(message);
        }

        // If sending failed or the entire message has been sent, delete the message
        if (!result || message->allRecipients.isEmpty())
        {
            messagesToRemove.append(message);
        }
    }

    // Delete any messages we no longer need. Can't do this in above foreach because altering a
    // collection while it is being iterated over can cause unexpected errors.
    BOOST_FOREACH (EncodedMessage *message, messagesToRemove)
    {
        pendingMessages.removeOne(message);
        delete message;
    }

    // Update any listeners
    emit messageStatusChanged(getMessageProgress());
}

const char* MessageModel::getPathToMessageFile() const
{
    return (GetDataDir() / "messages.dat").string().data();
}

bool MessageModel::loadPreviousMessages()
{
    if (!boost::filesystem::exists(getPathToMessageFile()))
    {
        return true;
    }

    // Block so the file is automatically closed
    {
        CAutoFile file(fopen(getPathToMessageFile(), "r"), SER_DISK, CLIENT_VERSION);

        if (!file)
        {
            return false;
        }

        pendingMessages.clear();

        try
        {
            file >> *this;
        }
        catch (std::exception)
        {
            return false;
        }
    }

    remove(getPathToMessageFile());

    return true;
}

bool MessageModel::saveCurrentMessages()
{
    {
        LOCK(cs_message);

        if (pendingMessages.isEmpty())
        {
            return true;
        }

        // Block so the file is automatically closed
        {
            CAutoFile file(fopen(getPathToMessageFile(), "w"), SER_DISK, CLIENT_VERSION);

            if (!file)
            {
                return false;
            }

            try
            {
                file << *this;
            }
            catch (std::exception)
            {
                return false;
            }
        }

        // Clean up messages
        BOOST_FOREACH (EncodedMessage *message, pendingMessages)
        {
            delete message;
        }

        pendingMessages.clear();
    }

    return true;
}
