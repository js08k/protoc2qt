/*
 * Copyright (c) , All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// GtQt Includes
#include "peerlink.h"
#include "tcpserver.h"

// Qt Includes
#include <QTimer>

__NAMESPACE__::PeerLink::PeerLink( QObject* parent )
    : QObject(parent)
    , m_allowMulti(false)
    , m_server(NULL)
{
    // Generate a new __NAMESPACE__::TcpServer with this as the parent
    m_server = new __NAMESPACE__::TcpServer(this);
    connect( m_server, SIGNAL(newConnection()), SLOT(newConnection()) );
}

__NAMESPACE__::PeerLink::~PeerLink()
{
    foreach( gtqt::TcpSocket* socket, m_peers )
        if ( socket ) { socket->deleteLater(); }

    m_peers.clear();

    m_server->deleteLater();
}

/*!
 * \brief PeerLink::close Close all active links and shutdown the server
 */
void __NAMESPACE__::PeerLink::close()
{
    foreach( gtqt::TcpSocket* socket, m_peers )
        if ( socket ) { socket->deleteLater(); }

    m_peers.clear();

    if ( m_server )
    { m_server->close(); }
}

/*!
 * \brief PeerLink::close Close the link to the Peer described
 */
void __NAMESPACE__::PeerLink::close(
        QHostAddress const& peeraddr, quint16 peerport )
{
    QString const key(peeraddr.toString()+":"+QString::number(peerport));

    // Check for an already active/known connection to the target host
    QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));

    if ( i != m_peers.end() )
    {
        // Found the socket, hold a local to the pointer of the socket
        __NAMESPACE__::TcpSocket* socket(*i);

        // Erase from the peer table
        m_peers.erase(i);

        // Start the disconnect process on the socket
        socket->disconnectFromHost();

        // Inform the socket to delete itself when the disconnect completes
        socket->deleteLater();
    }
}

void __NAMESPACE__::PeerLink::listen( QHostAddress const& addr, quint16 port )
{
    if ( m_server->isListening() )
    { m_server->close(); }

    m_server->listen( addr, port );
}

/*!
 * \brief PeerLink::connectToHost Performs a connection to a host acting as a
 * client socket rather than a server socket
 * \param dest The destination peer address
 * \param port The destination peer port
 */
void __NAMESPACE__::PeerLink::connectToHost(
        QHostAddress const& dest, quint16 port )
{
    QString const key(dest.toString()+":"+QString::number(port));

    // Check for an already active/known connection to the target host
    QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));
    if ( i != m_peers.end() )
    {
        // An already active/known connection to the target existed, close
        // this connection before proceeding.
        __NAMESPACE__::TcpSocket* socket(*i);

        // Disconnect all listening slots
        socket->disconnect();

        // Start the disconnect process on the socket
        socket->disconnectFromHost();

        // Inform the socket to delete itself when the disconnect completes
        connect( socket, SIGNAL(disconnected()),
                 socket, SLOT(deleteLater()) );

        // For full cleanup, delete in 30 seconds if disconnected
        // is never called
        QTimer::singleShot( 30000, socket, SLOT(deleteLater()) );

        // Erase from the peer table
        m_peers.erase(i);
    }

    if ( m_allowMulti || m_peers.empty() )
    {
        // Create a new socket to make a connection with
        __NAMESPACE__::TcpSocket* socket = new __NAMESPACE__::TcpSocket(this);

        // Save the new socket in the table
        m_peers[key] = socket;

        // Connect all message receivers from this socket
__REPEAT_START__
        connect( socket, SIGNAL(receive(__NAMESPACE__::DataPackage<__NAMESPACE__::__KEY__>)),
                 this, SIGNAL(receive(__NAMESPACE__::DataPackage<__NAMESPACE__::__KEY__>)));
__REPEAT_END__
        connect( socket, SIGNAL(connected()), this, SLOT(connected()) );
        connect( socket, SIGNAL(disconnected()), this, SLOT(disconnected()) );
        connect( socket, SIGNAL(error(QAbstractSocket::SocketError)),
                 this, SLOT(handle(QAbstractSocket::SocketError)) );

        // Connect the the server implementation peer
        socket->connectToHost(dest, port);
    }
}

/*!
 * \brief __NAMESPACE__::PeerLink::setAllowMulti Sets the allowMulti
 * flag. This flag controls whether the peer link will allow multiple
 * connections or not. This effects the server side by only accepting the first
 * connection, this effects the client side by only allowing connectToHost if no
 * connections are present
 * \param allowMulti
 */
void __NAMESPACE__::PeerLink::setAllowMulti( bool allowMulti )
{
    m_allowMulti = allowMulti;
}

void __NAMESPACE__::PeerLink::connected()
{
    if ( dynamic_cast<__NAMESPACE__::TcpSocket*>(sender()) )
    {
        __NAMESPACE__::TcpSocket* socket(
                    static_cast<__NAMESPACE__::TcpSocket*>(sender()) );

        QString const key(socket->peerAddress().toString()+":"+
                          QString::number(socket->peerPort()));

        // Check for the existance in the peer map
        QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));
        if ( i != m_peers.end() )
        {
            emit connected( socket->peerAddress(), socket->peerPort() );
        }
    }
}

void __NAMESPACE__::PeerLink::disconnected()
{
    if ( dynamic_cast<__NAMESPACE__::TcpSocket*>(sender()) )
    {
        __NAMESPACE__::TcpSocket* socket(
                    static_cast<__NAMESPACE__::TcpSocket*>(sender()) );

        QString const key(socket->peerAddress().toString()+":"+
                          QString::number(socket->peerPort()));

        // Check for the existance in the peer map
        QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));
        if ( i != m_peers.end() )
        {
            m_peers.erase(i);
            emit disconnected( socket->peerAddress(), socket->peerPort() );
        }

        socket->deleteLater();
    }
//  else if ( dynamic_cast<__NAMESPACE__::UdpSocket*>(sender()) )
//  {
//  }
}

void __NAMESPACE__::PeerLink::newConnection()
{
    __NAMESPACE__::TcpSocket* sock( m_server->nextPendingConnection() );

    QString const key(sock->peerAddress().toString()+":"+
                      QString::number(sock->peerPort()));

    // Check for an already active/known connection to the target host
    QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));
    if ( i != m_peers.end() )
    {
        // An already active/known connection to the target existed, close
        // this connection before proceeding.
        __NAMESPACE__::TcpSocket* socket(*i);

        // Erase from the peer table
        m_peers.erase(i);

        // Disconnect all listening slots
        socket->disconnect();

        // Start the disconnect process on the socket
        socket->disconnectFromHost();

        // Inform the socket to delete itself when the disconnect completes
        socket->deleteLater();
    }

    // Do not allow multiple connections, drop the new connection if our peer
    // list is non empty
    if ( !m_allowMulti && !m_peers.empty() )
    {
        // Disconnect the new socket
        sock->disconnectFromHost();

        // Free the socket's resources
        sock->deleteLater();
    }
    else
    {

        // Save the new socket in the table
        m_peers[key] = sock;

        // Connect all message receivers from this socket
__REPEAT_START__
        connect( sock, SIGNAL(receive(__NAMESPACE__::DataPackage<__NAMESPACE__::__KEY__>)),
                 this, SIGNAL(receive(__NAMESPACE__::DataPackage<__NAMESPACE__::__KEY__>)));
__REPEAT_END__
        connect( sock, SIGNAL(disconnected()), this, SLOT(disconnected()) );
        connect( sock, SIGNAL(error(QAbstractSocket::SocketError)),
                 this, SLOT(handle(QAbstractSocket::SocketError)) );

        emit connected( sock->peerAddress(), sock->peerPort() );
    }
}

void __NAMESPACE__::PeerLink::handle(QAbstractSocket::SocketError e)
{
    // Some error occured, remove the link in error from our link list, and
    // pass the error up
    if ( dynamic_cast<__NAMESPACE__::TcpSocket*>(sender()) )
    {
        __NAMESPACE__::TcpSocket* socket(
                    static_cast<__NAMESPACE__::TcpSocket*>(sender()) );

        QString const key(socket->peerAddress().toString()+":"+
                          QString::number(socket->peerPort()));

        // Check for the existance in the peer map
        QMap<QString,__NAMESPACE__::TcpSocket*>::iterator i(m_peers.find(key));
        if ( i != m_peers.end() )
        {
            m_peers.erase(i);
        }

        socket->deleteLater();
    }
//  else if ( dynamic_cast<__NAMESPACE__::UdpSocket*>(sender()) )
//  {
//  }

    emit error(e);
}

__REPEAT_START__
void __NAMESPACE__::PeerLink::transmit( __NAMESPACE__::DataPackage<__NAMESPACE__::__KEY__> const& data )
{
    foreach( __NAMESPACE__::TcpSocket* socket, m_peers )
        if ( socket ) { socket->write(data); }
}

__REPEAT_END__
