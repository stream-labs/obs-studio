#include <Wininet.h>
#include <string>
#include <vector>

#pragma comment(lib, "Wininet.lib")

static DWORD WSHTTPGenericRequestToStream(const std::string & a_URI, const std::string & a_method, const std::string & a_headers, 
	DWORD* a_pOutHttpCode, DWORD a_nTimeoutMS, std::string& response, const std::string& a_omoData = {}, const std::string& a_headerAcceptType = "application/octet-stream")
{
	response.clear();

	DWORD nValue = 128;
	::InternetSetOption(NULL, INTERNET_OPTION_MAX_CONNS_PER_SERVER, (LPVOID)&nValue, sizeof(nValue));

	DWORD retVal = ERROR_SUCCESS;

	// Parse URI
	URL_COMPONENTSA uri;
	uri.dwStructSize = sizeof(uri);
	uri.dwSchemeLength = 1;
	uri.dwHostNameLength = 1;
	uri.dwUserNameLength = 1;
	uri.dwPasswordLength = 1;
	uri.dwUrlPathLength = 1;
	uri.dwExtraInfoLength = 1;
	uri.lpszScheme  = NULL;
	uri.lpszHostName = NULL;
	uri.lpszUserName = NULL;
	uri.lpszPassword = NULL;
	uri.lpszUrlPath = NULL;
	uri.lpszExtraInfo = NULL;

	if (BOOL isProperURI = ::InternetCrackUrlA(a_URI.c_str(), (DWORD)a_URI.length(), 0, &uri))
	{
		std::string scheme(uri.lpszScheme, uri.dwSchemeLength);
		std::string serverName(uri.lpszHostName, uri.dwHostNameLength);
		std::string object(uri.lpszUrlPath, uri.dwUrlPathLength);
		std::string extraInfo(uri.lpszExtraInfo, uri.dwExtraInfoLength);
		std::string username(uri.lpszUserName, uri.dwUserNameLength);
		std::string password(uri.lpszPassword, uri.dwPasswordLength);

		// Open HTTP resource
		if (HINTERNET handle = ::InternetOpenA("HTTP", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0))
		{
			// Setup timeout
			if (::InternetSetOptionA(handle, INTERNET_OPTION_CONNECT_TIMEOUT, &a_nTimeoutMS, sizeof(DWORD)))
			{
				// Connect to server
				if (HINTERNET session = ::InternetConnectA(handle, serverName.c_str(), uri.nPort, username.c_str(), password.c_str(), INTERNET_SERVICE_HTTP, 0, 0))
				{
					// Request object
					DWORD dwFlags = INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD;

					if (scheme == "https")
					{
						dwFlags |= INTERNET_FLAG_SECURE;
						// dwFlags |= INTERNET_FLAG_KEEP_CONNECTION; Not needed for independent calls
					}

					// Define the Accept Types
					std::string sHeaders = a_headers;
					LPCSTR rgpszAcceptTypes[] = { sHeaders.c_str() , NULL };
					
					if (HINTERNET h_istream = ::HttpOpenRequestA(session, a_method.c_str(), (object + extraInfo).c_str(), NULL, NULL, rgpszAcceptTypes, dwFlags, 0))
					{
						// Send attached data, if any
						BOOL requestOk = TRUE;

						// Append input headers
						if (a_headers.length() > 0)
							requestOk = ::HttpAddRequestHeadersA(h_istream, a_headers.c_str(), (DWORD)a_headers.length(), HTTP_ADDREQ_FLAG_ADD);

						// Ignore certificate verification 
						DWORD nFlags;
						DWORD nBuffLen = sizeof(nFlags);
						::InternetQueryOptionA(h_istream, INTERNET_OPTION_SECURITY_FLAGS, (LPVOID)&nFlags, &nBuffLen);
						nFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
						::InternetSetOptionA(h_istream, INTERNET_OPTION_SECURITY_FLAGS, &nFlags, sizeof(nFlags));

						// Send
						if (a_omoData.size() > 0)
						{
							if (requestOk)
								requestOk = ::HttpSendRequestA(h_istream, NULL, 0, (void*)a_omoData.data(), a_omoData.size());
						}
						else if (requestOk)
						{
							requestOk = ::HttpSendRequestA(h_istream, NULL, 0, NULL, 0);
						}

						// Validate
						if (requestOk)
						{
							// Query HTTP status code
							if (a_pOutHttpCode != nullptr)
							{
								DWORD dwStatusCode = 0;
								DWORD dwSize = sizeof(dwStatusCode);

								*a_pOutHttpCode = 0;

								if (HttpQueryInfo(h_istream, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatusCode, &dwSize, NULL))
									*a_pOutHttpCode = dwStatusCode;
							}

							// Read response
							static const DWORD SIZE = 4 * 1024;
							BYTE data[ SIZE ];
							DWORD size = 0;

							// Download
							do 
							{
								// Read chunk of bytes
								BOOL result = ::InternetReadFile(h_istream, data, SIZE, &size);

								// Error check
								if (result)
								{ 
									// Write data read
									if (size > 0)
										response.append((char*)data, size);
								}
								else
								{
									retVal = GetLastError();
								}

							} while (retVal == ERROR_SUCCESS && size > 0);
						}
						else
						{
							retVal = GetLastError();
						}

						// Close handles
						::InternetCloseHandle(h_istream);
					}
					else
					{
						retVal = GetLastError();
					}

					::InternetCloseHandle(session);
				}
				else
				{
					retVal = GetLastError();
				}
			}
			else
			{
				retVal = GetLastError();
			}

			// Close internet handle
			::InternetCloseHandle(handle);
		}
		else
		{
			retVal = GetLastError();
		}
	}
	else
	{
		retVal = GetLastError();
		retVal = ERROR_INVALID_DATA;
	}

	return retVal;
}
