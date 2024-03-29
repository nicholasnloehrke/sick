//
// SPDX-License-Identifier: Unlicense
// 
// Created: November 2019
// 
// SICK AG, Waldkirch
// email: TechSupport0905@sick.de

#include "CoLa2ProtocolHandler.h"

#include <cassert>

#include "VisionaryEndian.h"
#include <iostream>

namespace visionary 
{

CoLa2ProtocolHandler::CoLa2ProtocolHandler(ITransport& rTransport)
  : m_rTransport(rTransport)
  , m_ReqID(0)
  , m_sessionID(0)
{
}

CoLa2ProtocolHandler::~CoLa2ProtocolHandler()
= default;

bool CoLa2ProtocolHandler::openSession(uint8_t sessionTimeout /*secs*/)
{
  // TODO: request session id and open session
  bool result =  true;
  std::vector<std::uint8_t> buffer = createCoLa2Header();
  buffer.push_back('O'); // Open Session
  buffer.push_back('x');
  buffer.push_back(sessionTimeout);  // sessionTimeout secs timeout
  buffer.insert(buffer.end(), { 0, 2, 'E', 'x' }); // Example flexstring, max 32 bytes
  // Overwrite length
  *reinterpret_cast<uint32_t*>(&buffer[4]) = nativeToBigEndian(static_cast<uint32_t>(buffer.size()) - 8);
  //
  // send to socket
  //

  m_rTransport.send(buffer);
  buffer.clear();

  //
  // get response
  //
  if(result)
  {
    const auto length = sizeof(uint32_t);
    if(m_rTransport.read(buffer, length) < static_cast<ITransport::recv_return_t>(length))
    {
      result = false;
    }
    else
    {
      // check for magic bytes
      const std::vector<uint8_t> MagicBytes = { 0x02, 0x02, 0x02, 0x02 };
      result = std::equal(MagicBytes.begin(), MagicBytes.end(), buffer.begin());
    }
  }
  
  if (result)
  {
    const auto length = sizeof(uint32_t);
    // get length
    if(m_rTransport.read(buffer, length) < static_cast<ITransport::recv_return_t>(length))
    {
      result = false;
    }
    
  }
  if (result)
  {
    const auto length = readUnalignBigEndian<uint32_t>(buffer.data());
    if(length < 4 || m_rTransport.read(buffer, length) < static_cast<ITransport::recv_return_t>(length))
    {
      result = false;
    }
    else
    {
      m_sessionID = readUnalignBigEndian<uint32_t>(buffer.data() + 2);
    }
  }
  return result;
}

void CoLa2ProtocolHandler::closeSession()
{
  // TODO: close session
}

uint16_t CoLa2ProtocolHandler::getReqId()
{
  return ++m_ReqID;
}

std::vector<std::uint8_t> CoLa2ProtocolHandler::createCoLa2Header()
{
  std::vector<std::uint8_t> header;

  // insert magic bytes
  const uint8_t MAGIC_BYTE = 0x02;
  // inserts 8 bytes at front (Magic Bytes and length)
  for (uint8_t i = 0; i < 8; i++)
  {
    header.push_back(MAGIC_BYTE);
  }
  // add HubCntr
  header.push_back(0); // TBD

  // add NoC
  header.push_back(0); // TBD

  // add SockIdx0
  //header.insert(header.end(), { 0, 0, 0, 1 }); // TBD

  // add SessionID
  header.insert(header.end(), { 0, 0, 0, 0 });
  *reinterpret_cast<uint32_t*>(&header[10]) = nativeToBigEndian(static_cast<uint32_t>(m_sessionID));
  // add ReqID
  header.insert(header.end(), { 0, 0 });
  *reinterpret_cast<uint16_t*>(&header[14]) = nativeToBigEndian(static_cast<uint16_t>(getReqId()));

  return header;
}

CoLaCommand CoLa2ProtocolHandler::send(CoLaCommand cmd)
{
  //
  // convert cola cmd to vector buffer and add/fill header
  //

  std::vector<std::uint8_t> buffer;
  buffer = cmd.getBuffer();

  std::vector<std::uint8_t> header = createCoLa2Header();

  buffer.erase(buffer.begin()); // remove 's' from CoLaCommand buffer, not used in CoLa2
  buffer.insert(buffer.begin(), header.begin(), header.end());
  header.clear();

  // Overwrite length
  assert(buffer.size() >=  4u + 4u + 8u);
  *reinterpret_cast<uint32_t*>(&buffer[4]) = nativeToBigEndian(static_cast<uint32_t>(buffer.size()) - 8);

  // Add checksum to end
  //buffer.insert(buffer.end(), calculateChecksum(buffer));

  //
  // send to socket
  //
  if (m_rTransport.send(buffer) != static_cast<ITransport::send_return_t>(buffer.size()))
  {
    return CoLaCommand::networkErrorCommand();
  }
  buffer.clear();

  //
  // get response
  //
  if (m_rTransport.read(buffer, sizeof(uint32_t)) != static_cast<ITransport::recv_return_t>(sizeof(uint32_t)))
  {
    return CoLaCommand::networkErrorCommand();
  }
  // check for magic bytes
  const std::vector<uint8_t> MagicBytes = { 0x02, 0x02, 0x02, 0x02 };
  bool result = std::equal(MagicBytes.begin(), MagicBytes.end(), buffer.begin());
  if (result)
  {
    // get length
    if (m_rTransport.read(buffer, sizeof(uint32_t)) != static_cast<ITransport::recv_return_t>(sizeof(uint32_t)))
    {
      return CoLaCommand::networkErrorCommand();
    }
    const auto length = readUnalignBigEndian<uint32_t>(buffer.data());
    if (m_rTransport.read(buffer, length) != static_cast<ITransport::recv_return_t>(length))
    {
      return CoLaCommand::networkErrorCommand();
    }
    if(buffer.size() > 8)
    {
      buffer.erase(buffer.begin(), buffer.begin() + 8); // drop header
    }
    buffer.insert(buffer.begin(), 's'); // insert 's'
  }
  else
  {
    // drop invalid data
    buffer.clear();
  }
  
  CoLaCommand response(buffer);
  return response;
}

uint8_t CoLa2ProtocolHandler::calculateChecksum(const std::vector<uint8_t>& buffer)
{
  uint8_t checksum = 0;
  for (size_t i = 8; i < buffer.size(); i++)
  {
    checksum ^= buffer[i];
  }
  return checksum;
}

}
