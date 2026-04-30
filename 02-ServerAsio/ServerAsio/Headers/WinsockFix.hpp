#pragma once

#ifndef WINSOCK_FIX_HPP_
#define WINSOCK_FIX_HPP_

// Prevent <windows.h> from including <winsock.h>, which conflicts with <winsock2.h>
#define _WINSOCKAPI_
#include <winsock2.h>
#include <windows.h>

#endif WINSOCK_FIX_HPP_
