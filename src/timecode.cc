/* Copyright 2018 Streampunk Media Ltd.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/* -LICENSE-START-
 ** Copyright (c) 2010 Blackmagic Design
 **
 ** Permission is hereby granted, free of charge, to any person or organization
 ** obtaining a copy of the software and accompanying documentation covered by
 ** this license (the "Software") to use, reproduce, display, distribute,
 ** execute, and transmit the Software, and to prepare derivative works of the
 ** Software, and to permit third-parties to whom the Software is furnished to
 ** do so, all subject to the following:
 **
 ** The copyright notices in the Software and this entire statement, including
 ** the above license grant, this restriction and the following disclaimer,
 ** must be included in all copies of the Software, in whole or in part, and
 ** all derivative works of the Software, unless such copies or derivative
 ** works are solely in the form of machine-executable object code generated by
 ** a source language processor.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 ** DEALINGS IN THE SOFTWARE.
 ** -LICENSE-END-
 */

#include "timecode.h"

macadamTimecode::macadamTimecode(
  uint16_t framesps,
  bool drop,
  uint8_t hours,
  uint8_t minutes,
  uint8_t seconds,
  uint8_t frames,
  uint8_t framePair) {

  fps = framesps;
  frameTab = new frameTable(framesps);
  if (drop) flags = flags | bmdTimecodeIsDropFrame;
  SetComponents(hours, minutes, seconds, frames, framePair);
}

BMDTimecodeBCD macadamTimecode::GetBCD() {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t frames;
  BMDTimecodeBCD bcdtc;
  HRESULT hresult;

  hresult = GetComponents(&hours, &minutes, &seconds, &frames);
  if (hresult != S_OK) {
    return (BMDTimecodeBCD) 0;
  }

  bcdtc = ((hours / 10) << 28) | ((hours % 10) << 24) |
    ((minutes / 10) << 20) | ((minutes % 10) << 16) |
    ((seconds / 10) << 12) | ((seconds % 10) << 8) |
    ((frames / 10) << 4) | frames % 10;
  return bcdtc;
}

HRESULT macadamTimecode::GetComponents (
  /* out */ uint8_t *hours,
  /* out */ uint8_t *minutes,
  /* out */ uint8_t *seconds,
  /* out */ uint8_t *frames) {

  if ((flags & bmdTimecodeIsDropFrame) == 0) {
    uint32_t baseTimecode = (fps > 30) ? value / 2 : value;
    *frames = baseTimecode % frameTab->scaledFps;
    uint32_t totalSeconds = baseTimecode / frameTab->scaledFps;
    *seconds = totalSeconds % 60;
    uint32_t totalMinutes = totalSeconds / 60;
    *minutes = totalMinutes % 60;
    *hours = totalMinutes / 60;

    if (fps > 30) {
      flags = flags & ~bmdTimecodeFieldMark;
      if ((value % 2) == 1) flags = flags | bmdTimecodeFieldMark;
    }
    return S_OK;
  }
  uint32_t baseTimecode = (fps > 30) ? value / 2 : value;
  *hours = baseTimecode / frameTab->dropFpHour;
  uint32_t remainingFrames = (uint32_t) (baseTimecode % frameTab->dropFpHour);
  uint32_t majorMinutes = remainingFrames / frameTab->dropFpMin10;
  remainingFrames = remainingFrames % frameTab->dropFpMin10;

  if (remainingFrames < (uint32_t) (frameTab->scaledFps * 60)) {
    *minutes = majorMinutes * 10;
    *seconds = remainingFrames / frameTab->scaledFps;
    *frames = remainingFrames % frameTab->scaledFps;
  }
  else {
    remainingFrames = remainingFrames - (frameTab->scaledFps * 60);
    *minutes = majorMinutes * 10 + remainingFrames / frameTab->dropFpMin + 1;
    remainingFrames = remainingFrames % frameTab->dropFpMin;
    if (remainingFrames < (uint32_t) (frameTab->scaledFps - 2)) { // Only the first second of a minute is short
      *seconds = 0;
      *frames = remainingFrames + 2;
    }
    else {
      remainingFrames += 2;
      *seconds = (remainingFrames / frameTab->scaledFps); // No 0 or 1 value.
      *frames = remainingFrames % frameTab->scaledFps;
    }
  }

  if (fps > 30) {
    flags = flags & ~bmdTimecodeFieldMark;
    if ((value % 2) == 1) flags = flags | bmdTimecodeFieldMark;
  }

  return S_OK;
}

HRESULT macadamTimecode::SetComponents (
  uint8_t hours,
  uint8_t minutes,
  uint8_t seconds,
  uint8_t frames,
  uint8_t framePair) {

  uint32_t tcv = 0;

  if ((flags & bmdTimecodeIsDropFrame) != 0) {
		tcv = (hours * frameTab->dropFpHour);
		tcv += ((minutes / 10) * frameTab->dropFpMin10);
		tcv += (minutes % 10) * frameTab->dropFpMin;
	}
	else {
		tcv = hours * frameTab->fpHour;
		tcv += minutes * frameTab->fpMinute;
	}

	tcv += seconds * ((fps > 30) ? fps / 2 : fps);
	tcv += frames;

  tcv = (fps > 30) ? tcv * 2 + framePair : tcv;
  value = tcv;

  return S_OK;
}

HRESULT macadamTimecode::formatTimecodeString(const char** timecode, bool fieldFlag) {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t frames;
  HRESULT hresult;

  hresult = GetComponents(&hours, &minutes, &seconds, &frames);

  hours = hours & 0x3fU;
  minutes = minutes & 0x3fU;
  seconds = seconds & 0x3fU;
  frames = frames & 0x3fU;

  char* tcstr;
  if (fieldFlag) {
    tcstr = (char *) malloc(14 * sizeof(char));
    sprintf(tcstr, "%02i:%02i:%02i%c%02i%s", hours, minutes, seconds,
      ((flags & bmdTimecodeIsDropFrame) != 0) ? ';' : ':', frames,
      ((flags & bmdTimecodeFieldMark) == 0) ? ".0" : ".1");
    tcstr[13] = '\0';
  }
  else {
    tcstr = (char *) malloc(12 * sizeof(char));
    sprintf(tcstr, "%02i:%02i:%02i%c%02i", hours, minutes, seconds,
      ((flags & bmdTimecodeIsDropFrame) != 0) ? ';' : ':', frames);
    tcstr[11] = '\0';
  }
  *timecode = (const char*) tcstr;

  return hresult;
}

#ifdef WIN32
HRESULT macadamTimecode::GetString (/* out */ BSTR *timecode) {
  const char* tcstr;
  HRESULT hresult;

  hresult = formatTimecodeString(&tcstr);
  _bstr_t btcstr(tcstr);
  *timecode = btcstr;
  return hresult;
}
#elif __APPLE__
HRESULT macadamTimecode::GetString (/* out */ CFStringRef *timecode) {
  const char* tcstr;
  HRESULT hresult;

  hresult = formatTimecodeString(&tcstr);
  CFStringRef cftcstr = CFStringCreateWithCString(nullptr, tcstr, kCFStringEncodingMacRoman);
  *timecode = cftcstr;
  return E_NOTIMPL;
}
#else
HRESULT macadamTimecode::GetString (/* out */ const char** timecode) {
  const char* tcstr;
  HRESULT hresult;

  hresult = formatTimecodeString(&tcstr);
  *timecode = tcstr;
  return hresult;
}
#endif

BMDTimecodeFlags macadamTimecode::GetFlags (void) {
  return flags;
}

HRESULT macadamTimecode::GetTimecodeUserBits (/* out */ BMDTimecodeUserBits *userBits) {
  *userBits = usrBts;
  return S_OK;
}

HRESULT macadamTimecode::SetTimecodeUserBits (BMDTimecodeUserBits userBits) {
  usrBts = userBits;
  return S_OK;
}

// Does not wrap around at 23:59:59:29.1
void macadamTimecode::Update(void) {
  value++;

  if (fps > 30) {
    flags = flags & ~bmdTimecodeFieldMark;
    if ((value % 2) == 1) flags = flags | bmdTimecodeFieldMark;
  }
}

HRESULT parseTimecode(uint16_t fps, const char* tcstr, macadamTimecode** timecode) {
  std::regex tcRe(
    "([0-9][0-9])[:;\\.,]([0-5][0-9])[:;\\.]([0-5][0-9])([:;\\.,])([0-5][0-9])(\\.[01])?",
    std::regex_constants::ECMAScript);
  std::cmatch match;

  std::regex_match(tcstr, match, tcRe);
  if ((match.empty() == true) || (match.size() < 6)) {
    return E_INVALIDARG;
  }

  *timecode = new macadamTimecode(
    fps,
    (match.str(4).compare(";") == 0) || (match.str(4).compare(",") == 0),
    (uint8_t) std::stoi(match.str(1)),
    (uint8_t) std::stoi(match.str(2)),
    (uint8_t) std::stoi(match.str(3)),
    (uint8_t) std::stoi(match.str(5)),
    (match.str(6).size() == 0) ? 0 : std::stoi(match.str(6).substr(1))
  );

  return S_OK;
}

napi_value timecodeTest(napi_env env, napi_callback_info info) {
  napi_status status;
  napi_value result;
  bool pass = true;
  macadamTimecode* tc;
  const char* tcstr = nullptr;
  uint8_t hours, minutes, seconds, frames;
  BMDTimecodeBCD bcd;

  tc = new macadamTimecode(30, true);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 0) && (minutes == 0) && (seconds == 0) && (frames == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 0) && (minutes == 0) && (seconds == 0) && (frames == 1);
  pass = pass && ((tc->GetFlags() & bmdTimecodeIsDropFrame) != 0);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  tc = new macadamTimecode(25);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 0) && (minutes == 0) && (seconds == 0) && (frames == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 0) && (minutes == 0) && (seconds == 0) && (frames == 1);
  pass = pass && ((tc->GetFlags() & bmdTimecodeIsDropFrame) == 0);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  tc = new macadamTimecode(30, true, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 13);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 14);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  tc = new macadamTimecode(60, true, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 13);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 13);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) != 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 14);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  tc = new macadamTimecode(60, true, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  bcd = tc->GetBCD();
  pass = pass && (bcd == 0x10111213);
  delete tc;

  tc = new macadamTimecode(25, false);
  pass = pass && (tc != nullptr);
  bcd = tc->GetBCD();
  pass = pass && (bcd == 0);
  delete tc;

  tc = new macadamTimecode(60, true, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->formatTimecodeString(&tcstr);
  pass = pass && (strcmp(tcstr, "10:11:12;13") == 0); // Zero is no difference
  delete tc;

  tc = new macadamTimecode(25, false, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->formatTimecodeString(&tcstr);
  pass = pass && (strcmp(tcstr, "10:11:12:13") == 0); // Zero is no difference
  delete tc;

  tc = new macadamTimecode(50, false, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->formatTimecodeString(&tcstr);
  pass = pass && (strcmp(tcstr, "10:11:12:13") == 0); // Zero is no difference
  delete tc;

  tc = new macadamTimecode(50, false, 10, 11, 12, 13);
  pass = pass && (tc != nullptr);
  tc->formatTimecodeString(&tcstr, true);
  pass = pass && (strcmp(tcstr, "10:11:12:13.0") == 0); // Zero is no difference
  delete tc;

  tc = new macadamTimecode(50, false, 10, 11, 12, 13, 1);
  pass = pass && (tc != nullptr);
  tc->formatTimecodeString(&tcstr, true);
  pass = pass && (strcmp(tcstr, "10:11:12:13.1") == 0); // Zero is no difference
  delete tc;

  pass = pass && (parseTimecode(30, "10:11:12:13", &tc) == S_OK);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 13);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  pass = pass && ((tc->GetFlags() & bmdTimecodeIsDropFrame) == 0);
  delete tc;

  pass = pass & (parseTimecode(60, "10:11:12;13.1", &tc) == S_OK);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 12) && (frames == 13);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) != 0);
  pass = pass && ((tc->GetFlags() & bmdTimecodeIsDropFrame) != 0);
  delete tc;

  tc = new macadamTimecode(60, true, 10, 11, 59, 29);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 59) && (frames == 29);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 59) && (frames == 29);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) != 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 12) && (seconds == 00) && (frames == 2);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 12) && (seconds == 00) && (frames == 02);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) != 0);
  delete tc;

  tc = new macadamTimecode(30, true, 10, 11, 59, 29);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 11) && (seconds == 59) && (frames == 29);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 12) && (seconds == 00) && (frames == 2);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  tc = new macadamTimecode(30, true, 10, 9, 59, 29);
  pass = pass && (tc != nullptr);
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 9) && (seconds == 59) && (frames == 29);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  tc->Update();
  tc->GetComponents(&hours, &minutes, &seconds, &frames);
  pass = pass && (hours == 10) && (minutes == 10) && (seconds == 00) && (frames == 0);
  pass = pass && ((tc->GetFlags() & bmdTimecodeFieldMark) == 0);
  delete tc;

  status = napi_get_boolean(env, pass, &result);
  CHECK_STATUS;
  return result;
};
