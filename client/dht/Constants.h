/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include <string>

namespace dht
{

static const int DEFAULT_DHT_PORT              = 6250;

static const int ID_BITS                       = 192;          // size of identifier (in bits)

static const unsigned CONNECTED_TIMEOUT        = 20*60*1000;   // when there hasn't been any incoming packet for this time, network will be set offline

static const unsigned SEARCH_ALPHA             = 15;           // degree of search parallelism
static const unsigned MAX_SEARCH_RESULTS       = 300;          // maximum of allowed search results
static const unsigned SEARCH_PROCESSTIME       = 3*1000;       // how often to process done search requests

static const unsigned SEARCH_STOPTIME          = 15*1000;      // how long to wait for delayed search results before deleting the search
static const unsigned SEARCHNODE_LIFETIME      = 20*1000;      // how long to try searching for node
static const unsigned SEARCHFILE_LIFETIME      = 90*1000;      // how long to try searching for file
static const unsigned SEARCHSTOREFILE_LIFETIME = 20*1000;      // how long to try publishing a file

static const unsigned SELF_LOOKUP_TIME         = 4*60*60*1000; // how often to search for self node
static const unsigned SELF_LOOKUP_TIME_INIT    = 3*60*1000;

static const unsigned K                        = 10;           // maximum nodes in one bucket

static const int MIN_PUBLISH_FILESIZE          = 1024 * 1024;  // 1 MiB, files below this size won't be published

static const unsigned REPUBLISH_TIME           = 5*60*60*1000; // when our filelist should be republished
static const unsigned PFS_REPUBLISH_TIME       = 1*60*60*1000; // when partially downloaded files should be republished
static const unsigned PUBLISH_TIME             = 2*1000;       // how often publishes files

static const int MAX_PUBLISHES_AT_TIME         = 3;            // how many files can be published at one time

static const unsigned FW_RESPONSES             = 3;            // how many UDP port checks are needed to detect we are firewalled
static const unsigned FWCHECK_TIME             = 1*60*60*1000; // how often request firewalled UDP check

static const unsigned NODE_RESPONSE_TIMEOUT    = 2*60*1000;    // node has this time to response else we ignore him/mark him as dead node
static const unsigned NODE_EXPIRATION          = 2*60*60*1000; // when node should be marked as possibly dead

static const unsigned TIME_FOR_RESPONSE        = 3*60*1000;    // node has this time to respond to request; after that response will be marked as unwanted

//#define DHT_FEATURE "DHT0"

static const std::string NetworkName = "DHT";

}

#endif	// _CONSTANTS_H
