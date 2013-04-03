#include "messagedialog.h"
#include "ui_messagedialog.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

#include <QMessageBox>

MessageDialog::MessageDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessageDialog)
{
    ui->setupUi(this);

    dateMismatch = false;

    // Set max dates to today
    ui->fromDate->setMaximumDate(QDate::currentDate());
    ui->toDate->setMaximumDate(QDate::currentDate());

    // Initialize date inputs to the current date
    ui->fromDate->setDate(QDate::currentDate());
    ui->toDate->setDate(QDate::currentDate());
}

MessageDialog::~MessageDialog()
{
    delete ui;
}

void MessageDialog::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
}

void MessageDialog::setMessageModel(MessageModel *messageModel)
{
    this->messageModel = messageModel;
}

void MessageDialog::on_searchButton_clicked()
{
    QString message;

    // Remove any previous text
    ui->decodeMessage->clear();

    // If no message is found
    if (!messageModel->searchForMessage(ui->decodeAddress->text(), ui->fromDate->date(), ui->toDate->date(), &message))
    {
        QMessageBox::warning(
                    this,
                    tr("No Message Found"),
                    tr("During the dates specified, no message was sent to the specified address."));
        return;
    }

    // If an invalid message is found
    if (message.length() == 0)
    {
        QMessageBox::warning(
                    this,
                    tr("No Valid Message Found"),
                    tr("Transactions payed to the specified address during the specified dates were found but they did not contain a valid message."));
        return;
    }

    ui->decodeMessage->setPlainText(message);
}

void MessageDialog::on_decodeAddress_textChanged(const QString &arg1)
{
    // Disable the search button if the search dates are invalid or there the address textbox is empty
    ui->searchButton->setEnabled(!dateMismatch && !arg1.isEmpty());
}

void MessageDialog::on_fromDate_dateChanged(const QDate &date)
{
    // If the from date is after the to date
    if (date > ui->toDate->date())
    {
        dateMismatch = true;
        ui->fromDate->setStyleSheet("QDateEdit { background: red }");
        ui->searchButton->setDisabled(true);
    }
    else
    {
        dateMismatch = false;
        ui->fromDate->setStyleSheet("QDateEdit { background: white }");
        ui->toDate->setStyleSheet("QDateEdit { background: white }");
        ui->searchButton->setEnabled(!ui->decodeAddress->text().isEmpty());
    }
}

void MessageDialog::on_toDate_dateChanged(const QDate &date)
{
    // If the to date is before the from date
    if (date < ui->fromDate->date())
    {
        dateMismatch = true;
        ui->toDate->setStyleSheet("QDateEdit { background: red }");
        ui->searchButton->setDisabled(true);
    }
    else
    {
        dateMismatch = false;
        ui->fromDate->setStyleSheet("QDateEdit { background: white }");
        ui->toDate->setStyleSheet("QDateEdit { background: white }");
        ui->searchButton->setEnabled(!ui->decodeAddress->text().isEmpty());
    }
}

void MessageDialog::on_encodeMessage_textChanged()
{
    // Removes invalid characters
    int cursorPos = ui->encodeMessage->textCursor().position();
    QString text = ui->encodeMessage->toPlainText().toLower(); // Only lower case characters
    QString cleanText;

    for (int i = 0; i < text.size(); i++)
    {
        // Only allow letters and spaces
        if ((text[i].isLetter() || text[i].isSpace()) && text[i] != '\n')
        {
            cleanText.append(text[i]);
        }
        else
        {
            cursorPos--;
        }
    }

    // Disconnect the signal so the slot doesn't fire when we change the text
    disconnect(ui->encodeMessage, SIGNAL(textChanged()), this, SLOT(on_encodeMessage_textChanged()));

    ui->encodeMessage->setPlainText(cleanText);

    // Reset the cursor position
    QTextCursor cursor = ui->encodeMessage->textCursor();
    cursor.setPosition(cursorPos);
    ui->encodeMessage->setTextCursor(cursor);

    // Reconnect the signal
    connect(ui->encodeMessage, SIGNAL(textChanged()), this, SLOT(on_encodeMessage_textChanged()));

    // A required field might have changed
    encodingRequiredFieldsChanged();
}

void MessageDialog::on_encodeAddress_textChanged()
{
    encodingRequiredFieldsChanged();
}

void MessageDialog::on_encodeButton_clicked()
{
    // At least 1 BTC is required to encode messages
    if (walletModel->getBalance() < 100000000)
    {
        QMessageBox::warning(
                    this,
                    tr("Too Few Bitcoins"),
                    tr("You need at least 1 BTC to be able to encode a message."));
        return;
    }

    // If the address is invalid
    if (!walletModel->validateAddress(ui->encodeAddress->text()))
    {
        QMessageBox::warning(
                    this,
                    tr("Invalid Address"),
                    tr("The specified address is not valid."));
        return;
    }

    MessageModel::EncodedMessage message;

    // If there is a problem creating the message
    if (!messageModel->initializeMessage(ui->encodeMessage->toPlainText(), ui->encodeAddress->text(), message))
    {
        QMessageBox::warning(
                    this,
                    tr("Unable To Encode Message"),
                    tr("The message specified cannot be encoded. Please try again with a different message."));
        return;
    }

    // If the address does not belong to the user, ensure the user has enough bitcoins
    // and ask for confirmation to send the coins
    if (!message.isSendToSelf())
    {
        if (message.getAmount() > walletModel->getBalance())
        {
            QMessageBox::warning(
                        this,
                        tr("Insufficient Funds"),
                        tr("Encoding this message requires %1. You do not have enough bitcoins.")
                            .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), message.getAmount())));
            return;
        }
        else if (!QMessageBox::question(
                     this,
                     tr("Send Coins Confirmation"),
                     tr("Are you sure you want to send %1?")
                         .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), message.getAmount())),
                     QMessageBox::Ok,
                     QMessageBox::Cancel))
         {
             return;
         }
    }

    // If for some reason sending the message failed. The message model handles notifying the user.
    if (!messageModel->sendMessage(message))
    {
        return;
    }

    QMessageBox::information(
                this,
                tr("Message Encoded"),
                tr("The message was succesfully encoded and is being sent. It may take a significant amount of time before all the transactions are added to the block chain."));

    ui->encodeMessage->clear();
}

void MessageDialog::encodingRequiredFieldsChanged()
{
    // Disable the encode button if either the address or the message text boxes are empty
    ui->encodeButton->setDisabled(ui->encodeAddress->text().isEmpty() || ui->encodeMessage->toPlainText().isEmpty());
}
