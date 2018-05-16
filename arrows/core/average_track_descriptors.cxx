/*ckwg +29
 * Copyright 2018 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither name of Kitware, Inc. nor the names of any contributors may be used
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "average_track_descriptors.h"

#include <deque>

namespace kwiver {
namespace arrows {
namespace core {


// ==================================================================================
class average_track_descriptors::priv
{
public:
  priv( average_track_descriptors* parent )
    : m_parent( parent )
    , m_logger( kwiver::vital::get_logger( "average_track_descriptors" ) )
    , m_rolling( false )
    , m_interval( 5 )
  { }

  ~priv() { }

  average_track_descriptors* m_parent;
  kwiver::vital::logger_handle_t m_logger;
  bool m_rolling;
  unsigned int m_interval;

  std::map< vital::track_id_t, std::deque< std::vector< double > > > m_history;
};


// ==================================================================================
average_track_descriptors
::average_track_descriptors()
  : d( new priv( this ) )
{
}


// ----------------------------------------------------------------------------------
average_track_descriptors
::~average_track_descriptors()
{
}


// ----------------------------------------------------------------------------------
void
average_track_descriptors
::set_configuration( vital::config_block_sptr config )
{
  d->m_rolling = config->get_value< bool >( "rolling", d->m_rolling );
  d->m_interval = config->get_value< unsigned int >( "interval", d->m_interval );
}


// ----------------------------------------------------------------------------------
bool
average_track_descriptors
::check_configuration( vital::config_block_sptr config ) const
{
  return true;
}


// ----------------------------------------------------------------------------------
vital::track_descriptor_set_sptr
average_track_descriptors
::compute( vital::timestamp ts,
           vital::image_container_sptr image_data,
           vital::object_track_set_sptr tracks )
{
  vital::track_descriptor_set_sptr tds( new vital::track_descriptor_set() );

  if( tracks )
  {
    for( vital::track_sptr track : tracks->tracks() )
    {
      if( !track )
      {
        LOG_WARN( d->m_logger, "Warning: Invalid Track" );
        continue;
      }

      vital::track::history_const_itr it = track->find( ts.get_frame() );
      if( it != track->end() )
      {
        std::deque< std::vector< double > >& average_history = d->m_history[ track->id() ];

        std::shared_ptr< vital::object_track_state > ots =
          std::dynamic_pointer_cast< vital::object_track_state >( *it );

        if( ots && ots->detection && ots->detection->descriptor() )
        {
          average_history.push_back( ots->detection->descriptor()->as_double() );
          if( average_history.size() == d->m_interval )
          {
            std::vector< double > average;
            std::size_t i = 0;

            bool done = false;
            while( true )
            {
              double total = 0.0;

              for( std::vector< double >& entry : average_history )
              {
                if( i >= entry.size() )
                {
                  done = true;
                  break;
                }

                total += entry[ i ];
              }

              if( done )
              {
                break;
              }
              else
              {
                average.push_back( total / average_history.size() );
                i++;
              }
            }

            if( d->m_rolling )
            {
              average_history.pop_front();
            }
            else
            {
              average_history.clear();
            }

            vital::track_descriptor_sptr td = vital::track_descriptor::create( "cnn_descriptor" );

            td->add_track_id( track->id() );

            vital::track_descriptor::descriptor_data_sptr data(
              new vital::track_descriptor::descriptor_data_t( average.size() ) );
            std::copy( average.begin(), average.end(), data->raw_data() );
            td->set_descriptor( data );

            // Make history entry
            vital::track_descriptor::history_entry he( ts, ots->detection->bounding_box() );
            td->add_history_entry( he );

            tds->push_back( td );
          }
        }
      }
    }
  }

  return tds;
}


// ----------------------------------------------------------------------------------
vital::track_descriptor_set_sptr
average_track_descriptors
::flush()
{
  return vital::track_descriptor_set_sptr( new vital::track_descriptor_set() );
}


} } }