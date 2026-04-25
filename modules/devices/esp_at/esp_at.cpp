/**
 ******************************************************************************
 * @file
 * @author  Samuel Almeida Rocha
 * @version
 * @date
 * @ingroup
 * @brief   
 *
 ******************************************************************************
 */

#include "esp_at.hpp"

namespace hel
{

EspAt::EspAt(Uart& handle, Gpio& resetPin)
    :
    StaticTask("Esp32", TaskPriority::Normal),
    AtClient(handle),
    m_fsm(this),
    m_resetPin(resetPin)
{
  using URCCallbeck = Callback<void(String&)>;

  AtClient::registerURCCallback("ready", URCCallbeck(this, &EspAt::eventReady));
  AtClient::registerURCCallback("WIFI CONNECTED", URCCallbeck(this, &EspAt::eventConnect));
  AtClient::registerURCCallback("WIFI GOT IP", URCCallbeck(this, &EspAt::eventTime));
  AtClient::registerURCCallback("+MQTTCONNECTED", URCCallbeck(this, &EspAt::mqttConnEvets));
  AtClient::registerURCCallback("+MQTTRECV", URCCallbeck(this, &EspAt::mqttRecvEvent));
  AtClient::registerURCCallback("+CWLAP:", URCCallbeck(this, &EspAt::listApEvent));
}

void EspAt::run() noexcept
{

  for(;;)
  {
    processURC();
  }
}

void EspAt::eventReady(String& msg)
{

}

void EspAt::eventConnect(String& msg)
{

}

void EspAt::eventTime(String& msg)
{

}


void EspAt::mqttConnEvets(String& msg)
{

}


void EspAt::mqttRecvEvent(String& msg)
{

}


void EspAt::listApEvent(String& msg)
{

}



} /* namespace dev */
