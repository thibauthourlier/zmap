/*  File: zmapDataSource_P.h
 *  Author: Steve Miller (sm23@sanger.ac.uk)
 *  Copyright (c) 2006-2015: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * and was written by
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *         Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *
 * Description:
 * Exported functions:
 *-------------------------------------------------------------------
 */

#ifndef DATA_SOURCE_P_H
#define DATA_SOURCE_P_H


#include <zmapDataSource.hpp>


#ifdef USE_HTSLIB

/*
 * HTS header. Temporary location.
 */
/* #include "/nfs/users/nfs_s/sm23/Work/htslib-develop/htslib/hts.h"
#include "/nfs/users/nfs_s/sm23/Work/htslib-develop/htslib/sam.h" */
#include <htslib/hts.h>
#include <htslib/sam.h>

#endif


struct bed ;


/*
 * General source type
 */
class ZMapDataSourceStruct
{
public:
  ZMapDataSourceStruct(const GQuark source_name, const char *sequence) ;
  virtual ~ZMapDataSourceStruct() {} ;

  virtual bool isOpen() = 0 ;
  virtual bool checkHeader() = 0 ;
  virtual bool readLine(GString * const pStr) = 0 ;
  virtual bool gffVersion(int * const p_out_val) ;

  GError* error() ;

  ZMapDataSourceType type ;

protected:
  GQuark source_name_ ;
  char *sequence_ ;
  GError *error_ ;
} ;

/*
 *  Full declarations of concrete types to represent the GIO channel and
 *  HTS file data sources.
 */
class ZMapDataSourceGIOStruct : public ZMapDataSourceStruct
{
public:
  ZMapDataSourceGIOStruct(const GQuark source_name, const char *file_name, const char *open_mode, const char *sequence) ;
  ~ZMapDataSourceGIOStruct() ;

  bool isOpen() ;
  bool checkHeader() ;
  bool readLine(GString * const pStr) ;
  bool gffVersion(int * const p_out_val) ;

  GIOChannel *io_channel ;
} ;

class ZMapDataSourceBEDStruct : public ZMapDataSourceStruct
{
public:
  ZMapDataSourceBEDStruct(const GQuark source_name, const char *file_name, const char *open_mode, const char *sequence) ;
  ~ZMapDataSourceBEDStruct() ;

  bool isOpen() ;
  bool checkHeader() ;
  bool readLine(GString * const pStr) ;

  //private:
  struct bed* bed_features_ ;
  struct bed* cur_feature_ ;
} ;

class ZMapDataSourceBIGBEDStruct : public ZMapDataSourceStruct
{
public:
  ZMapDataSourceBIGBEDStruct(const GQuark source_name, const char *file_name, const char *open_mode, 
                             const char *sequence, const int start, const int end) ;
  ~ZMapDataSourceBIGBEDStruct() ;

  bool isOpen() ;
  bool checkHeader() ;
  bool readLine(GString * const pStr) ;

private:
  int start_ ; // sequence region start
  int end_ ;   // sequence region end
  struct bbiFile *bbi_file_ ;
  struct lm *lm_; // Memory pool to hold returned list from bbi file
  struct bigBedInterval *list_ ;
  struct bigBedInterval *cur_interval_ ; // current item from list_
} ;

class ZMapDataSourceBIGWIGStruct : public ZMapDataSourceStruct
{
public:
  ZMapDataSourceBIGWIGStruct(const GQuark source_name, const char *file_name, const char *open_mode, 
                             const char *sequence, const int start, const int end) ;
  ~ZMapDataSourceBIGWIGStruct() ;

  bool isOpen() ;
  bool checkHeader() ;
  bool readLine(GString * const pStr) ;

private:
  int start_ ; // sequence region start
  int end_ ;   // sequence region end
  struct bbiFile *bbi_file_ ;
  struct lm *lm_; // Memory pool to hold returned list from bbi file
  struct bbiInterval *list_ ;
  struct bbiInterval *cur_interval_ ; // current item from list_
} ;


#ifdef USE_HTSLIB

class ZMapDataSourceHTSStruct : public ZMapDataSourceStruct
{
public:
  ZMapDataSourceHTSStruct(const GQuark source_name, const char *file_name, const char *open_mode, const char *sequence) ;
  ~ZMapDataSourceHTSStruct() ;
  bool isOpen() ;
  bool checkHeader() ;
  bool readLine(GString * const pStr) ;

  htsFile *hts_file ;
  /* bam header and record object */
  bam_hdr_t *hts_hdr ;
  bam1_t *hts_rec ;

  /*
   * Data that we need to store
   */
  char *so_type ;
} ;

#endif


typedef ZMapDataSourceGIOStruct *ZMapDataSourceGIO ;
typedef ZMapDataSourceBEDStruct *ZMapDataSourceBED ;
typedef ZMapDataSourceBIGBEDStruct *ZMapDataSourceBIGBED ;
typedef ZMapDataSourceBIGWIGStruct *ZMapDataSourceBIGWIG ;
#ifdef USE_HTSLIB
typedef ZMapDataSourceHTSStruct *ZMapDataSourceHTS ;
#endif


/*
 * This is for temporary data that need to be stored while
 * records are being read from a file. At the moment this is used
 * only for HTS data sources, since we need a struct allocated to
 * read records into.
 */
typedef struct ZMapDataSourceTempStorageStruct_
  {
    ZMapDataSourceType type ;
  } ZMapDataSourceTempStorageStruct, *ZMapDataSourceTempStorage ;

typedef struct ZMapDataSourceTempStorageStructGIO_
  {
    ZMapDataSourceType type ;
  } ZMapDataSourceTempStorageStructGIO, *ZMapDataSourceTempStorageGIO ;

typedef struct ZMapDataSourceTempStorageStructHTS_
  {
    ZMapDataSourceType type ;
  } ZMapDataSourceTempStorageStructHTS, *ZMapDataSourceTempStorageHTS ;


#endif
