#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include "messagemodel.h"

#include <QWidget>
#include <QDate>

namespace Ui {
    class MessageDialog;
}

class MessageDialog : public QWidget
{
    Q_OBJECT
    
public:
    explicit MessageDialog(QWidget *parent = 0);
    ~MessageDialog();

    void setWalletModel(WalletModel *walletModel);
    void setMessageModel(MessageModel *messageModel);
    
private slots:
    void on_searchButton_clicked();
    void on_decodeAddress_textChanged(const QString &arg1);
    void on_fromDate_dateChanged(const QDate &date);
    void on_toDate_dateChanged(const QDate &date);
    void on_encodeMessage_textChanged();
    void on_encodeAddress_textChanged();
    void on_encodeButton_clicked();

private:
    Ui::MessageDialog *ui;
    bool dateMismatch;
    WalletModel *walletModel;
    MessageModel *messageModel;

    void encodingRequiredFieldsChanged();
};

#endif // MESSAGEDIALOG_H
