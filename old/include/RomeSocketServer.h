#ifndef ROMESOCKETSERVER_H_
#define ROMESOCKETSERVER_H_

#include "RomeSocket.h"

class RomeSocketServer : public RomeSocket
{
private:
    int connection;
    int max_conntion = 200;

public:
    ERROR_CODE Accept();

    virtual ERROR_CODE CreateSocket(const IP_VERSION &ip_version,
                                    const SOCKET_TYPE &protocol,
                                    const unsigned &port,
                                    const std::string &ip = "") override;

    virtual ERROR_CODE SendVector(BYTES &data) const override;
    virtual ERROR_CODE ReceiveVector(BYTES &data) const override;
    virtual ERROR_CODE SendString(const std::string &data) const override;
    virtual ERROR_CODE ReceiveString(std::string &data) const override;

    virtual ~RomeSocketServer() override;
};

#endif