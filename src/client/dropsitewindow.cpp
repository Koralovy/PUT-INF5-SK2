/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>

#include "droparea.h"
#include "dropsitewindow.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <sstream>

//! [recive any string message]
int recv_msg(char** buffer, size_t sizeof_buffer, SOCKET cfd)
{
    free(*buffer);
    *buffer = NULL;
    *buffer = (char*)malloc(sizeof(char)*sizeof_buffer);
    int rc = ::recv(cfd, *buffer, sizeof_buffer, 0);
    while(memchr((void*)*buffer, '\n', rc) == NULL)
    {
        rc += ::recv(cfd, (*buffer)+rc, sizeof_buffer-rc, 0);
        qDebug() << rc << buffer;
    }
    return rc;
}

//! [communication accept signal]
void ack_wait(SOCKET sfd)
{
    char *ack_buf = (char*)malloc(sizeof(char)*5);
    recv_msg(&ack_buf,5,sfd);
    while(strncmp(ack_buf, "ack\n", 4)!=0)
        recv_msg(&ack_buf,5,sfd);
    qDebug() << ack_buf;
    free(ack_buf);
    ack_buf = (char*)malloc(sizeof(char)*5);
}

void DropSiteWindow::log_message(QString message)
{
    qDebug() << message;
    textBrowser->append(message.replace("\n", ""));
}

//! [Main window]
DropSiteWindow::DropSiteWindow()
{
    //input form
    abstractLabel = new QLabel(tr("Dane połączenia"));
    abstractLabel->setWordWrap(true);
    abstractLabel->adjustSize();
    formLayout = new QFormLayout();
    ipLineEdit = new QLineEdit("localhost");
    portLineEdit = new QLineEdit("2137");
    nameLineEdit = new QLineEdit("archive.zip");
    formLayout->addRow(tr("Ip:"), ipLineEdit);
    formLayout->addRow(tr("Port:"), portLineEdit);
    formLayout->addRow(tr("Nazwa archiwum:"), nameLineEdit);

    //QT BSD licensed example - drop area
    dropArea = new DropArea;
    connect(dropArea, &DropArea::changed,
            this, &DropSiteWindow::updateFormatsTable);

    //Table with archive path, size and hidden absolute path
    QStringList labels;
    labels << tr("Ścieżka") << tr("Rozmiar");

    formatsTable = new QTableWidget;
    formatsTable->setColumnCount(3);
    formatsTable->setColumnHidden(2, true);
    formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setHorizontalHeaderLabels(labels);
    formatsTable->horizontalHeader()->setStretchLastSection(true);

    //app buttons
    sendButton = new QPushButton(tr("Wyślij"));
    sendButton->setEnabled(false);
    quitButton = new QPushButton(tr("Quit"));

    buttonBox = new QDialogButtonBox;
    buttonBox->addButton(sendButton, QDialogButtonBox::ActionRole);

    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);

    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    connect(sendButton, &QAbstractButton::clicked, this, &DropSiteWindow::send);

    textBrowser = new QTextBrowser();
    textBrowser -> setVisible(false);

    //build interface from widgets
    mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(abstractLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(dropArea);
    mainLayout->addWidget(formatsTable);
    mainLayout->addWidget(textBrowser);
    mainLayout->addWidget(buttonBox);

    setWindowTitle(tr("Kompresowanie plików"));
    setMinimumSize(375, 500);
}


//! [Drop area and table operator]
void DropSiteWindow::updateFormatsTable(const QMimeData *mimeData)
{
    if (!mimeData)
        return;
    const QStringList formats = mimeData->formats();
    QString text;
    for (const QString &format : formats)
    {
        if (format == QLatin1String("text/uri-list"))
        {
            //unlock send button after adding some data
            sendButton->setEnabled(true);

            //append data to table
            QTableWidgetItem *formatItem = new QTableWidgetItem(format);
            formatItem->setFlags(Qt::ItemIsEnabled);
            formatItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

            if (format == QLatin1String("text/uri-list"))
            {
                QList<QUrl> urlList = mimeData->urls();
                for (int i = 0; i < urlList.size() && i < 32; ++i)
                    text.append(urlList.at(i).toString());
            }
            text = text.remove(0, 8);
            qDebug() << text;
            if (QDir(text).exists())
            { //if dir
               QDirIterator it(text, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks); //get all files
               while (it.hasNext())
               {
                   QString filePath = it.next();
                   if (!QDir(filePath).exists())
                   {
                       int row = formatsTable->rowCount();
                       formatsTable->insertRow(row);
                       int fsize = QFileInfo(filePath).size();

                       qDebug() << QFileInfo(filePath);
                       qDebug() << fsize;
                       qDebug() << filePath.remove(text.left(text.lastIndexOf('/')));

                       formatsTable->setItem(row, 0, new QTableWidgetItem(filePath.remove(text)));
                       formatsTable->setItem(row, 1, new QTableWidgetItem(QString::number(fsize)));
                       formatsTable->setItem(row, 2, new QTableWidgetItem(filePath));

                   }
               }
            }
            else
            { // if file
                int row = formatsTable->rowCount();
                qDebug() << text << QFileInfo(text).size();
                formatsTable->insertRow(row);
                formatsTable->setItem(row, 0, new QTableWidgetItem(text.mid(text.lastIndexOf('/')+1)));
                formatsTable->setItem(row, 1, new QTableWidgetItem(QString::number(QFileInfo(text).size())));
                formatsTable->setItem(row, 2, new QTableWidgetItem(text));
            }
            formatsTable->sortByColumn(0, Qt::AscendingOrder);
            return;
        }
    }

    //focus table formatting on file names
    formatsTable->resizeColumnToContents(0);

}

//! [create commands for server]
void DropSiteWindow::crawl()
{
    std::vector<std::string> cmd;
    std::vector<std::vector<std::string>> paths;
    std::vector<std::string> filenames;
    std::vector<int> sizes;

    //prepare all files to needed format - get path, filename and size from gui
    for(int i=0; i<formatsTable->rowCount(); ++i)
    {
        std::stringstream ss(formatsTable->item(i, 0)->text().toStdString());
        std::string str;
        std::vector<std::string> dirs;
        while (getline(ss,str,'/'))
        {
            if(str!="")
            {
                dirs.push_back(str);
            }
        }
        std::string filename = dirs.back();
        dirs.pop_back();
        paths.push_back(dirs);
        filenames.push_back(filename);
        sizes.push_back(formatsTable->item(i, 1)->text().toInt());
    }

    //generate commands for all files
    for(auto i : paths)
    {
        std::stringstream c;
        //command for getting into the right directory
        for(auto j : i)
            c << "d "<< j<<std::endl;
        size_t vec_size = cmd.end()-cmd.begin();
        //check if relative folder in two messages are the same, if so then dont't do extra traverse
        if(vec_size >= 3)
        {
            if(c.str() != cmd[vec_size-3])
            {
                cmd.push_back(c.str());

            }
            else
            {
                cmd.pop_back();
            }
        }
        else
        {
            cmd.push_back(c.str());
        }

        //file transfer command
        c = std::stringstream("");
        c<<"f "<<*filenames.begin()<<" "<<*sizes.begin()<<std::endl;
        filenames.erase(filenames.begin());
        sizes.erase(sizes.begin());
        cmd.push_back(c.str());
        c = std::stringstream("");
        //if needed go back to previous dirs
        for(auto j : i)
            c << "d .."<<std::endl;

        cmd.push_back(c.str());
    }
    //split commands with multiple newlines to final command execution set
    std::vector<std::string> final_cmd;
    std::string x;
    for(auto i : cmd)
    {
        std::stringstream ss(i);
        while(getline(ss,x))
            final_cmd.push_back(x + '\n');

    }

    crawl_and_generate = final_cmd;
}

//! [send button]
void DropSiteWindow::send()
{
    //show log text box
    textBrowser->setVisible(true);
    formatsTable->setVisible(false);
    dropArea->setVisible(false);
    sendButton->setVisible(false);
    mainLayout ->update();

    WORD WRequiredVersion;
    WSADATA WData;
    SOCKET SSocket;
    int nConnect;
    struct sockaddr_in stServerAddr;
    struct hostent* lpstServerEnt;

    // initialize winsock
    WRequiredVersion = MAKEWORD(2, 0);
    if (WSAStartup(WRequiredVersion, &WData) != 0)
    {
        log_message("WSAStartup failed!");
        return;
    }

    // look up server's IP address
    lpstServerEnt = gethostbyname(ipLineEdit->text().toStdString().c_str());
    if (! lpstServerEnt)
    {
        log_message(ipLineEdit->text() + QString(": Can't get the server's IP address.\n"));
        return;
    }
    // create a socket
    SSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // server configuration
    memset(&stServerAddr, 0, sizeof(struct sockaddr));
    stServerAddr.sin_family = AF_INET;
    memcpy(&stServerAddr.sin_addr.s_addr, lpstServerEnt->h_addr, lpstServerEnt->h_length);
    stServerAddr.sin_port = htons(portLineEdit->text().toInt());

    // connect to the server
    nConnect = ::connect(SSocket, (struct sockaddr*)&stServerAddr, sizeof(struct sockaddr));
    if (nConnect < 0)
    {
        log_message("Can't connect to the server.\n");
        return;
    }

    // ask for archive to be created
    std::stringstream ss;
    ss << "a " << nameLineEdit->text().toStdString() << "\n";
    log_message(QString(ss.str().c_str()));
    ::send(SSocket, ss.str().c_str(), ss.str().size()+1, 0);
    ack_wait(SSocket);

    int iter = 0;
    //generate all commands for file transfer
    crawl();
    for(auto i: crawl_and_generate)
    {
        log_message(i.c_str() + QString::number(i.size()));
        ::send(SSocket, i.c_str(), i.size()+1, 0);
        ack_wait(SSocket);


        if (i[0] == 'f')
        {
            qDebug() << formatsTable->item(iter, 2)->text().toStdString().c_str();
            //open file
            FILE* fp = ::fopen(formatsTable->item(iter, 2)->text().toStdString().c_str(), "rb");
            if(fp == NULL)
            {
                log_message("File " + formatsTable->item(iter, 2)->text() + " read error, exitting\n");
                return;
            }
            fseek(fp, 0L, SEEK_END);
            size_t archive_size = ftell(fp);
            rewind(fp);
            char* archive = (char*)malloc(sizeof(char)*archive_size);
            //put file in memory and send it
            fread(archive, sizeof(char), archive_size, fp);
            ::send(SSocket, archive, archive_size, 0);
            ack_wait(SSocket);

            free(archive);
            archive = NULL;
            iter++;
        }
    }

    //ask for archive file
    std::stringstream().swap(ss);
    ss << "a end" << "\n";
    log_message(ss.str().c_str());
    ::send(SSocket, ss.str().c_str(), ss.str().size()+1, 0);
    ack_wait(SSocket);

    //get returned filename
    char* msg_buffer = (char*)malloc(sizeof(char)*1000);
    recv_msg(&msg_buffer, 1000, SSocket);
    msg_buffer = strtok(msg_buffer, (const char*)"\n");
    qDebug() << msg_buffer;

    if(msg_buffer[0]=='f')
    {
        const char delim[] = " ";
        char* token;
        token = strtok(msg_buffer+2*sizeof(char), delim);
        char* filename = token;
        token = strtok(NULL, delim);
        size_t size = atoi(token);
        log_message("file tokenized description: \n\tfilename: " + QString(filename) + " \n\tsize: " + QString::number(size));

        //inform server about being ready to recive archive
        ::send(SSocket, "ack\n", 5, 0);
        char* file = (char*)malloc(sizeof(char)*size);

        //receive archive
        int rc = ::recv(SSocket, file, size, 0);
        while((unsigned long long)rc!=size)
        {
            qDebug() << "\tdoing repeats after " << rc << "bytes\n";
            rc += ::recv(SSocket, file+rc, size-rc, 0);
        }

        //save archive to file
        FILE* fp  = fopen(filename, "wb");
        fwrite(file, size,sizeof(char), fp);
        free(file);
        fclose(fp);
        ::send(SSocket, "ack\n", 5, 0);
    }
    else
    {
        log_message("Unexpected message from server, exitting\n");
        return;
    }

    //close connection
    closesocket(SSocket);
    WSACleanup();
}



