/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "EmptyHeader.h"

#ifdef RAKNET_SOCKET_2_INLINE_FUNCTIONS

#ifndef RAKNETSOCKET2_WINDOWS_LINUX_CPP
#define RAKNETSOCKET2_WINDOWS_LINUX_CPP

#if !defined(WINDOWS_STORE_RT) && !defined(__native_client__)

#if RAKNET_SUPPORT_IPV6==1

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <system_error>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

void PrepareAddrInfoHints2(addrinfo* hints)
{
	memset(hints, 0, sizeof(addrinfo)); // make sure the struct is empty
	hints->ai_socktype = SOCK_DGRAM; // UDP sockets
	hints->ai_flags = AI_PASSIVE;     // fill in my IP for me
}

void GetMyIP_Windows_Linux_IPV4And6(SystemAddress addresses[MAXIMUM_NUMBER_OF_INTERNAL_IDS]) {
	char hostname[80];
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		throw std::system_error(errno, std::system_category(), "gethostname");
	}

	addrinfo hints;
	PrepareAddrInfoHints2(&hints);

	addrinfo* servinfo = nullptr;
	int error = getaddrinfo(hostname, "", &hints, &servinfo);
	if (error != 0) {
		throw std::system_error(error, std::system_category(),
			"getaddrinfo: " + std::string(gai_strerror(error)));
	}

	int idx = 0;
	for (addrinfo* aip = servinfo; aip != nullptr && idx < MAXIMUM_NUMBER_OF_INTERNAL_IDS;
		aip = aip->ai_next, ++idx) {
		if (aip->ai_family == AF_INET) {
			sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(aip->ai_addr);
			std::memcpy(&addresses[idx].address.addr4, ipv4, sizeof(sockaddr_in));
		}
		else {
			sockaddr_in6* ipv6 = reinterpret_cast<sockaddr_in6*>(aip->ai_addr);
			std::memcpy(&addresses[idx].address.addr6, ipv6, sizeof(sockaddr_in6));
		}
	}

	freeaddrinfo(servinfo);

	//std::fill(addresses.begin() + idx, addresses.end(), UNASSIGNED_SYSTEM_ADDRESS);

	while (idx < MAXIMUM_NUMBER_OF_INTERNAL_IDS)
	{
		addresses[idx] = UNASSIGNED_SYSTEM_ADDRESS;
		idx++;
	}
}

#else

#if (defined(__GNUC__)  || defined(__GCCXML__)) && !defined(__WIN32__)
#include <netdb.h>
#endif
void GetMyIP_Windows_Linux_IPV4( SystemAddress addresses[MAXIMUM_NUMBER_OF_INTERNAL_IDS] )
{



	int idx=0;
	char ac[ 80 ];
	int err = gethostname( ac, sizeof( ac ) );
    (void) err;
	RakAssert(err != -1);
	
	//struct hostent *phe =  getaddrinfo(ac);  //gethostbyname( ac );

	//get our hostname, using getaddrinfo
	struct addrinfo hints;
	struct addrinfo *servinfo=0, *aip;  // will point to the results
	getaddrinfo(ac, "", &hints, &servinfo);

	struct hostent* phe = 0;
	for (aip = servinfo; aip != NULL; aip = aip->ai_next)
	{
		if (aip->ai_family == AF_INET)
		{
			phe = gethostbyaddr((char*)&((struct sockaddr_in *)aip->ai_addr)->sin_addr.s_addr, sizeof(struct in_addr), AF_INET);
			break;
		}
	}

	if (phe==0)
	{
		// Try again, but use gethostbyname
		phe = gethostbyname( ac );
	}

	if ( phe == 0 )
	{
		RakAssert(phe!=0);
		return ;
	}
	for ( idx = 0; idx < MAXIMUM_NUMBER_OF_INTERNAL_IDS; ++idx )
	{
		if (phe->h_addr_list[ idx ] == 0)
			break;

		memcpy(&addresses[idx].address.addr4.sin_addr,phe->h_addr_list[ idx ],sizeof(struct in_addr));
	}
	
	while (idx < MAXIMUM_NUMBER_OF_INTERNAL_IDS)
	{
		addresses[idx]=UNASSIGNED_SYSTEM_ADDRESS;
		idx++;
	}

}

#endif // RAKNET_SUPPORT_IPV6==1


void GetMyIP_Windows_Linux( SystemAddress addresses[MAXIMUM_NUMBER_OF_INTERNAL_IDS] )
{
	#if RAKNET_SUPPORT_IPV6==1
		GetMyIP_Windows_Linux_IPV4And6(addresses);
	#else
		GetMyIP_Windows_Linux_IPV4(addresses);
	#endif
}


#endif // Windows and Linux

#endif // file header

#endif // #ifdef RAKNET_SOCKET_2_INLINE_FUNCTIONS
