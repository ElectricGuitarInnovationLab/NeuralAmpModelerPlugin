#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "json.hpp"

#if defined(OS_MAC)
#include <CFNetwork/CFNetwork.h>
#elif defined(OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace update_checker
{
struct Release
{
  std::string version;
  std::string url;
};

inline bool ParseVersion(const std::string& input, std::vector<int>& components)
{
  components.clear();
  size_t pos = 0;
  while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos])))
    ++pos;
  if (pos < input.size() && (input[pos] == 'v' || input[pos] == 'V'))
    ++pos;

  while (pos < input.size())
  {
    if (!std::isdigit(static_cast<unsigned char>(input[pos])))
      return false;

    int value = 0;
    while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos])))
    {
      value = (value * 10) + (input[pos] - '0');
      ++pos;
    }
    components.push_back(value);

    if (pos == input.size() || input[pos] == '-' || input[pos] == '+')
      break;
    if (input[pos] != '.')
      return false;
    ++pos;
  }

  return !components.empty();
}

inline bool IsNewerVersion(const std::string& candidate, const std::string& current)
{
  std::vector<int> candidateParts;
  std::vector<int> currentParts;
  if (!ParseVersion(candidate, candidateParts) || !ParseVersion(current, currentParts))
    return false;

  const auto count = std::max(candidateParts.size(), currentParts.size());
  candidateParts.resize(count);
  currentParts.resize(count);
  return candidateParts > currentParts;
}

inline bool ParseLatestRelease(const std::string& response, Release& release)
{
  try
  {
    const auto json = nlohmann::json::parse(response);
    if (json.value("draft", true) || json.value("prerelease", true))
      return false;

    release.version = json.value("tag_name", "");
    release.url = json.value("html_url", "");
    std::vector<int> ignored;
    constexpr const char* kReleaseURLPrefix =
      "https://github.com/ElectricGuitarInnovationLab/Puke-Amp/releases/";
    return ParseVersion(release.version, ignored) && release.url.rfind(kReleaseURLPrefix, 0) == 0;
  }
  catch (const nlohmann::json::exception&)
  {
    return false;
  }
}

#if defined(OS_MAC)
inline bool HttpGet(const char* urlString, std::string& response)
{
  bool success = false;
  CFURLRef url = CFURLCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(urlString),
                                     static_cast<CFIndex>(strlen(urlString)), kCFStringEncodingUTF8, nullptr);
  if (!url)
    return false;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CFHTTPMessageRef request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), url, kCFHTTPVersion1_1);
  if (request)
  {
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Accept"), CFSTR("application/vnd.github+json"));
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("User-Agent"), CFSTR("Puke-Amp-Update-Checker"));
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("X-GitHub-Api-Version"), CFSTR("2022-11-28"));

    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, request);
    if (stream)
    {
      CFReadStreamSetProperty(stream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);
      if (CFReadStreamOpen(stream))
      {
        UInt8 buffer[4096];
        CFIndex count = 0;
        while ((count = CFReadStreamRead(stream, buffer, sizeof(buffer))) > 0 && response.size() < 1024 * 1024)
          response.append(reinterpret_cast<const char*>(buffer), static_cast<size_t>(count));

        auto* header = static_cast<CFHTTPMessageRef>(
          const_cast<void*>(CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader)));
        if (header)
        {
          success = count == 0 && CFHTTPMessageGetResponseStatusCode(header) == 200;
          CFRelease(header);
        }
        CFReadStreamClose(stream);
      }
      CFRelease(stream);
    }
    CFRelease(request);
  }
#pragma clang diagnostic pop
  CFRelease(url);
  return success;
}
#elif defined(OS_WIN)
inline bool HttpGet(const char* urlString, std::string& response)
{
  const int wideLength = MultiByteToWideChar(CP_UTF8, 0, urlString, -1, nullptr, 0);
  if (wideLength <= 0)
    return false;
  std::wstring wideUrl(static_cast<size_t>(wideLength), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, urlString, -1, wideUrl.data(), wideLength);

  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts))
    return false;

  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  const std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  HINTERNET session = WinHttpOpen(L"Puke Amp Update Checker", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return false;
  WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

  HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
  HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                       parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                                   : nullptr;
  const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
  bool success = request && WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L),
                                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                 WinHttpReceiveResponse(request, nullptr);

  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  success = success && WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                           WINHTTP_NO_HEADER_INDEX) && status == 200;
  while (success && response.size() < 1024 * 1024)
  {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available))
    {
      success = false;
      break;
    }
    if (available == 0)
      break;
    std::string chunk(available, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(request, chunk.data(), available, &read))
    {
      success = false;
      break;
    }
    response.append(chunk.data(), read);
  }

  if (request)
    WinHttpCloseHandle(request);
  if (connection)
    WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  return success;
}
#else
inline bool HttpGet(const char*, std::string&) { return false; }
#endif

inline bool FetchLatestRelease(Release& release)
{
  std::string response;
  constexpr const char* kLatestReleaseURL =
    "https://api.github.com/repos/ElectricGuitarInnovationLab/Puke-Amp/releases/latest";
  return HttpGet(kLatestReleaseURL, response) && ParseLatestRelease(response, release);
}
} // namespace update_checker
