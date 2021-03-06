/*

Copyright (c) 2003, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_TORRENT_HANDLE_HPP_INCLUDED
#define TORRENT_TORRENT_HANDLE_HPP_INCLUDED

#include <vector>

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <boost/date_time/posix_time/posix_time.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "libtorrent/peer_id.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/piece_picker.hpp"
#include "libtorrent/torrent_info.hpp"

namespace libtorrent
{
	namespace detail
	{
		struct session_impl;
		struct checker_impl;
	}

	struct duplicate_torrent: std::exception
	{
		virtual const char* what() const throw()
		{ return "torrent already exists in session"; }
	};

	struct invalid_handle: std::exception
	{
		virtual const char* what() const throw()
		{ return "invalid torrent handle used"; }
	};

	struct torrent_status
	{
		torrent_status()
			: state(queued_for_checking)
			, paused(false)
			, progress(0.f)
			, total_download(0)
			, total_upload(0)
			, total_payload_download(0)
			, total_payload_upload(0)
			, download_rate(0)
			, upload_rate(0)
			, download_payload_rate(0)
			, upload_payload_rate(0)
			, num_peers(0)
			, num_complete(-1)
			, num_incomplete(-1)
			, pieces(0)
			, total_done(0)
			, total_wanted_done(0)
			, total_wanted(0)
			, num_seeds(0)
			, distributed_copies(0.f)
			, block_size(0)
		{}

		enum state_t
		{
			queued_for_checking,
			checking_files,
			connecting_to_tracker,
			downloading_metadata,
			downloading,
			finished,
			seeding
		};
		
		state_t state;
		bool paused;
		float progress;
		boost::posix_time::time_duration next_announce;
		boost::posix_time::time_duration announce_interval;

		std::string current_tracker;

		// transferred this session!
		// total, payload plus protocol
		size_type total_download;
		size_type total_upload;

		// payload only
		size_type total_payload_download;
		size_type total_payload_upload;

		// the amount of payload bytes that
		// has failed their hash test
		size_type total_failed_bytes;

		// current transfer rate
		// payload plus protocol
		float download_rate;
		float upload_rate;

		// the rate of payload that is
		// sent and received
		float download_payload_rate;
		float upload_payload_rate;

		// the number of peers this torrent
		// is connected to.
		int num_peers;

		// if the tracker sends scrape info in its
		// announce reply, these fields will be
		// set to the total number of peers that
		// have the whole file and the total number
		// of peers that are still downloading
		int num_complete;
		int num_incomplete;

		const std::vector<bool>* pieces;

		// the number of bytes of the file we have
		// including pieces that may have been filtered
		// after we downloaded them
		size_type total_done;

		// the number of bytes we have of those that we
		// want. i.e. not counting bytes from pieces that
		// are filtered as not wanted.
		size_type total_wanted_done;

		// the total number of bytes we want to download
		// this may be smaller than the total torrent size
		// in case any pieces are filtered as not wanted
		size_type total_wanted;

		// the number of peers this torrent is connected to
		// that are seeding.
		int num_seeds;

		// the number of distributed copies of the file.
		// note that one copy may be spread out among many peers.
		//
		// the whole number part tells how many copies
		//   there are of the rarest piece(s)
		//
		// the fractional part tells the fraction of pieces that
		//   have more copies than the rarest piece(s).
		float distributed_copies;

		// the block size that is used in this torrent. i.e.
		// the number of bytes each piece request asks for
		// and each bit in the download queue bitfield represents
		int block_size;
	};

	struct partial_piece_info
	{
		enum { max_blocks_per_piece = piece_picker::max_blocks_per_piece };
		int piece_index;
		int blocks_in_piece;
		std::bitset<max_blocks_per_piece> requested_blocks;
		std::bitset<max_blocks_per_piece> finished_blocks;
		address peer[max_blocks_per_piece];
		int num_downloads[max_blocks_per_piece];
	};

	struct torrent_handle
	{
		friend class invariant_access;
		friend class session;
		friend class torrent;

		torrent_handle(): m_ses(0), m_chk(0) {}

		void get_peer_info(std::vector<peer_info>& v) const;
		bool send_chat_message(address ip, std::string message) const;
		torrent_status status() const;
		void get_download_queue(std::vector<partial_piece_info>& queue) const;

		std::vector<announce_entry> const& trackers() const;
		void replace_trackers(std::vector<announce_entry> const&);

		bool has_metadata() const;
		const torrent_info& get_torrent_info() const;
		bool is_valid() const;

		bool is_seed() const;
		bool is_paused() const;
		void pause();
		void resume();


		// marks the piece with the given index as filtered
		// it will not be downloaded
		void filter_piece(int index, bool filter);
		void filter_pieces(std::vector<bool> const& pieces);
		bool is_piece_filtered(int index) const;
		std::vector<bool> filtered_pieces() const;

		//idea from Arvid and MooPolice
		//todo refactoring and improving the function body
		// marks the file with the given index as filtered
		// it will not be downloaded
		void filter_file(int index, bool filter);
		void filter_files(std::vector<bool> const& files);

		// set the interface to bind outgoing connections
		// to.
		void use_interface(const char* net_interface);

		entry write_resume_data() const;

		// kind of similar to get_torrent_info() but this
		// is low level, returning the exact info-part of
		// the .torrent file. When hashed, this buffer
		// will produce the info hash. The reference is valid
		// only as long as the torrent is running.
		std::vector<char> const& metadata() const;

		// forces this torrent to reannounce
		// (make a rerequest from the tracker)
		void force_reannounce() const;

		// forces a reannounce in the specified amount of time.
		// This overrides the default announce interval, and no
		// announce will take place until the given time has
		// timed out.
		void force_reannounce(boost::posix_time::time_duration) const;

		// TODO: add a feature where the user can tell the torrent
		// to finish all pieces currently in the pipeline, and then
		// abort the torrent.

		void set_upload_limit(int limit);
		void set_download_limit(int limit);

		// manually connect a peer
		void connect_peer(address const& adr) const;

		// valid ratios are 0 (infinite ratio) or [ 1.0 , inf )
		// the ratio is uploaded / downloaded. less than 1 is not allowed
		void set_ratio(float up_down_ratio);

		// TODO: add finish_file_allocation, which will force the
		// torrent to allocate storage for all pieces.

		boost::filesystem::path save_path() const;

		// -1 means unlimited unchokes
		void set_max_uploads(int max_uploads);

		// -1 means unlimited connections
		void set_max_connections(int max_connections);

		void set_tracker_login(std::string const& name, std::string const& password);

		// post condition: save_path() == save_path if true is returned
		bool move_storage(boost::filesystem::path const& save_path);

		const sha1_hash& info_hash() const
		{ return m_info_hash; }

		bool operator==(const torrent_handle& h) const
		{ return m_info_hash == h.m_info_hash; }

		bool operator!=(const torrent_handle& h) const
		{ return m_info_hash != h.m_info_hash; }

		bool operator<(const torrent_handle& h) const
		{ return m_info_hash < h.m_info_hash; }

	private:

		torrent_handle(detail::session_impl* s,
			detail::checker_impl* c,
			const sha1_hash& h)
			: m_ses(s)
			, m_chk(c)
			, m_info_hash(h)
		{
			assert(m_ses != 0);
		}

#ifndef NDEBUG
		void check_invariant() const;
#endif

		detail::session_impl* m_ses;
		detail::checker_impl* m_chk;
		sha1_hash m_info_hash;

	};


}

#endif // TORRENT_TORRENT_HANDLE_HPP_INCLUDED
