/*
    spotyxbmc2 - A project to integrate Spotify into XBMC
    Copyright (C) 2011  David Erenger

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    For contact with the author:
    david.erenger@gmail.com
*/

#include "../session/Session.h"
#include "Codec.h"
#include "Util.h"
#include "utils/URIUtils.h"
#include <stdint.h>
#include "../radio/RadioHandler.h"

using namespace std;

namespace addon_music_spotify {

Codec::Codec() {
  m_SampleRate = 44100;
  m_Channels = 2;
  m_BitsPerSample = 16;
  m_Bitrate = 320;
  m_CodecName = "spotify";
  m_TotalTime = 0;
  m_currentTrack = 0;
  m_isPlayerLoaded = false;
  m_buffer = 0;
}

Codec::~Codec() {
  DeInit();
  delete m_buffer;
}

bool Codec::Init(const CStdString & strFile, unsigned int filecache) {
  m_bufferSize = 2048 * sizeof(int16_t) * 2 * 10;
  m_buffer = new char[m_bufferSize];
  CStdString uri = URIUtils::GetFileName(strFile);
  CStdString extension = uri.Right(uri.GetLength() - uri.Find('.') - 1);
  if (extension.Left(12) == "spotifyradio"){
   //if its a readiotrack the radionumber and tracknumber is secretly encoded at the end of the extension
    CStdString trackStr = extension.Right(extension.GetLength() - extension.ReverseFind('#') - 1);
    Logger::printOut(extension);
    CStdString radioNumber = extension.Left(uri.Find('#'));
    Logger::printOut(radioNumber);
    radioNumber = radioNumber.Right(radioNumber.GetLength() - radioNumber.Find('#') - 1);
    Logger::printOut("loading codec radio");
    RadioHandler::getInstance()->pushToTrack(atoi(radioNumber),atoi(trackStr));
  }
  //we have a non legit extension so remove it manually
  uri = uri.Left(uri.Find('.'));

  Logger::printOut("trying to load track:");
  Logger::printOut(uri);
  sp_link *spLink = sp_link_create_from_string(uri);
  m_currentTrack = sp_link_as_track(spLink);
  sp_track_add_ref(m_currentTrack);
  sp_link_release(spLink);
  m_totalTime = 0.001 * sp_track_duration(m_currentTrack);
  m_endOfTrack = false;
  m_bufferPos = 0;
  m_startStream = false;
  m_isPlayerLoaded = false;
  return true;
}

void Codec::DeInit() {
  unloadPlayer();
}

__int64 Codec::Seek(__int64 iSeekTime) {
  return 0;
}

int Codec::ReadPCM(BYTE *pBuffer, int size, int *actualsize) {
  *actualsize = 0;
  if (!m_isPlayerLoaded)
    loadPlayer();

  if (m_startStream) {
    if (m_endOfTrack && m_bufferPos == 0) {
      return READ_EOF;
    } else if (m_bufferPos > 0) {
      int amountToMove = m_bufferPos;
      if (m_bufferPos > size)
        amountToMove = size;
      memcpy(pBuffer, m_buffer, amountToMove);
      memmove(m_buffer, m_buffer + amountToMove, m_bufferSize - amountToMove);
      m_bufferPos -= amountToMove;
      *actualsize = amountToMove;
    }
  }
  return READ_SUCCESS;
}

bool Codec::CanInit() {
  return true;
}

void Codec::endOfTrack() {
  m_endOfTrack = true;
}

bool Codec::loadPlayer() {
  Logger::printOut("load player");
  if (!m_isPlayerLoaded) {
    //do we have a track at all?
    if (m_currentTrack) {
      CStdString name;
      Logger::printOut("load player 2");
      if (sp_track_is_loaded(m_currentTrack)) {
        sp_error error = sp_session_player_load(getSession(), m_currentTrack);
        CStdString message;
        Logger::printOut("load player 3");
        message.Format("%s", sp_error_message(error));
        Logger::printOut(message);
        Logger::printOut("load player 4");
        if (SP_ERROR_OK == error) {
          sp_session_player_play(getSession(), true);
          m_isPlayerLoaded = true;
          Logger::printOut("load player 5");
          return true;
        }
      }
    } else
      return false;
  }
  return true;
}

bool Codec::unloadPlayer() {
  if (m_isPlayerLoaded) {
    sp_session_player_play(getSession(), false);
    sp_session_player_unload(getSession());
  }

  if (m_currentTrack) {
    sp_track_release(m_currentTrack);
  }

  m_currentTrack = 0;
  m_isPlayerLoaded = false;
  m_endOfTrack = true;
  return true;
}

int Codec::musicDelivery(int channels, int sample_rate, const void *frames, int num_frames) {
  //Logger::printOut("music delivery");
  int amountToMove = num_frames * (int) sizeof(int16_t) * channels;

  if ((m_bufferPos + amountToMove) >= m_bufferSize) {
    amountToMove = m_bufferSize - m_bufferPos;
    //now the buffer is full, start playing
    m_startStream = true;
  }
  m_channels = channels;
  m_sampleRate = sample_rate;
  m_bitrate = 320;
  memcpy(m_buffer + m_bufferPos, frames, amountToMove);
  m_bufferPos += amountToMove;

  return amountToMove / ((int) sizeof(int16_t) * channels);
}

sp_session* Codec::getSession() {
  return Session::getInstance()->getSpSession();
}
}

/* namespace addon_music_spotify */