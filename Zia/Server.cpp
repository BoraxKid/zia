//
// Created by xobtah on 23/01/17.
//

#include <cstring>
#include <string>
#include <iostream>

#include "Server.hpp"
#include "ProtocolException.hpp"

namespace Zia
{
    /*
     *  Ctor & Dtor
     */

    Server::Server(DataBase<ClientStruct> &db, int port, int queue)
    try : _clients(db, [](ClientStruct &c) { delete c.stream; }), _acceptor(port, queue) {}
    catch (ProtocolException const &pe) { throw ServerException("ProtocolException: " + std::string(pe.what())); }

    Server::~Server() {}

    /*
     *  Public member functions
     */

    bool    Server::Select(uint8_t *packet, ClientStruct **client, unsigned int usec_timeout) try
    {
        fd_set  rfs;
        int max = this->CreateFdSet(&rfs);
        timeval timeout = { 0 };
        sockaddr_in csin = { 0 };

        memset(packet, 0, PACKET_MAX_SIZE);
        timeout.tv_sec = 0;
        timeout.tv_usec = usec_timeout;
        switch (this->OSSelect(max, &rfs, NULL, &timeout))
        {
            case -1:
                return (false);
            case 0:
                return (false);
            default:
                break;
        }
        if (_acceptor.IsInFdSet(&rfs))
        {
            this->NewClient();
            return (false);
        }
        *client = &_clients.FindOne([](const ClientStruct &c, void *rfs)
                                    {
                                        return (c.stream->IsInFdSet(reinterpret_cast<fd_set*>(rfs)));
                                    }, &rfs);
        if (!(*client)->stream->Recv(packet, PACKET_MAX_SIZE) || !static_cast<uint16_t>(*packet))
        {
            this->RemoveClient((*client)->_id);
            return (false);
        }
        return (true);
    }
    catch (ProtocolException const &pe) { throw ServerException("ProtocolException: " + std::string(pe.what())); }
    catch (DataBaseException const &dbe) { std::cerr << "Select: " + std::string(dbe.what()) << std::endl; return (false); }

    int     Server::SendPacket(ClientStruct const &client, uint8_t *packet, unsigned int size)
    {
        return (client.stream->Send(packet, size));
    }

    /*
     *  Private membre functions
     */

    int Server::CreateFdSet(fd_set *fs)
    {
        std::vector<ClientStruct*>    subdb = _clients.Find([](const ClientStruct &c)
                                                            {
                                                                static_cast<void>(c);
                                                                return (true);
                                                            });

        FD_ZERO(fs);
        _acceptor.AddInFdSet(fs);
        for (std::vector<ClientStruct*>::iterator it = subdb.begin(); it != subdb.end(); it++)
            (*it)->stream->AddInFdSet(fs);
        return (static_cast<int>(subdb.size() + 4));
    }

    int Server::OSSelect(int maxfd, fd_set *readfd, fd_set *writefd, struct timeval *to, fd_set *exceptfd)
    { return (select(maxfd, readfd, writefd, exceptfd, to)); }

    void    Server::NewClient() try
    {
        unsigned int    id = 0;
        ClientStruct c(id, _acceptor.Accept());

        std::cout << "New client" << std::endl;
        id = _clients.Insert(c);
        _clients.FindOne(id)._id = id;
    }
    catch (ProtocolException const &pe) { throw ServerException("ProtocolException: " + std::string(pe.what())); }

    void    Server::RemoveClient(unsigned int _id) try
    {
        _clients.Remove(_id);
        std::cout << "Rem client" << std::endl;
    }
    catch (DataBaseException const &dbe) { static_cast<void>(dbe); }
}