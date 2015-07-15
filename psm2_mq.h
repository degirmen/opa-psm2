/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2015 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  Intel Corporation, www.intel.com

  BSD LICENSE

  Copyright(c) 2015 Intel Corporation.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* Copyright (c) 2003-2014 Intel Corporation. All rights reserved. */

#ifndef PSM_MQ_H
#define PSM_MQ_H

#include <psm2.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @file psm_mq.h
 * @brief PSM Matched Queues
 *
 * @page psm_mq Matched Queues interface
 *
 * The Matched Queues (MQ) interface implements a queue-based communication
 * model with the distinction that queue message consumers use a 3-tuple of
 * metadata to match incoming messages against a list of preposted receive
 * buffers.  These semantics are consistent with those presented by MPI-1.2
 * and all the features and side-effects of Message-Passing find their way into
 * Matched Queues. There is currently a single MQ context,
 * If need be, MQs may expose a function to allocate more than
 * one MQ context in the future.  Since an MQ is implicitly bound to a locally
 * opened endpoint, handle all MQ functions use an MQ handle instead of an EP
 * handle as a communication context.
 *
 * @section tagmatch MQ Tag Matching
 *
 * A successful MQ tag match requires an endpoint address (@ref psm_epaddr_t)
 * and a 3-tuple of tag objects.  Two of the tag objects are provided by the
 * receiver when posting a receive buffer (@ref psm_mq_irecv) and the last is
 * provided by the sender as part of every message sent (@ref psm_mq_send and
 * @ref psm_mq_isend).  Since MQ is a receiver-directed communication model,
 * the tag matching done at the receiver involves matching the sent message's
 * origin and send tag (@c stag) with the source endpointer address, tag (@c
 * rtag), and tag selector (@c rtagsel) attached to every preposted receive
 * buffer.  The incoming @c stag is compared to the posted @c rtag but only for
 * significant bits set to @c 1 in the @c rtagsel.  The @c rtagsel can be used
 * to mask off parts (or even all) of the bitwise comparison between sender and
 * receiver tags.  A successful match causes the message to be received into
 * the buffer with which the tag is matched.  If the incoming message is too
 * large, it is truncated to the size of the posted receive buffer.  The
 * bitwise operation corresponding to a successful match and receipt of an
 * expected message amounts to the following expression evaluating as true:
 *
 *      @verbatim ((stag ^ rtag) & rtagsel) == 0 @endverbatim
 *
 * It is up to the user to encode (pack) into the 64-bit unsigned
 * integers, including employing the @c rtagsel tag selector as a method to
 * wildcart part or all of the bits significant in the tag matching operation.
 * For example, MPI uses triple based on context (MPI communicator), source
 * rank, send tag. The following code example shows how the triple can be
 * packed into 64 bits:
 *
 * @verbatim
 *    //
 *    // 64-bit send tag formed by packing the triple:
 *    //
 *    // ( context_id_16bits | source_rank_16bits | send_tag_32bits )
 *    //
 *    stag = ( (((context_id)&0xffffULL)<<48)|    \
 *             (((source_rank)&0xffffULL)<<32)|   \
 *             (((send_tag)&0xffffffffULL)) );
 * @endverbatim
 *
 * Similarly, the receiver applies the @c rtag matching bits and @rtagsel
 * masking bits against a list of send tags and returns the first successful
 * match.  Zero bits in the @c tagsel can be used to indicate wildcarded bits
 * in the 64-bit tag which can be useful for implementing MPI's
 * @c MPI_ANY_SOURCE and @c MPI_ANY_TAG.  Following the example bit splicing in
 * the above @c stag example:
 *
 * @verbatim
 * // Example MPI implementation where MPI_COMM_WORLD implemented as 0x3333
 *
 * // MPI_Irecv source_rank=MPI_ANY_SOURCE, tag=7, comm=MPI_COMM_WORLD
 * rtag    = 0x3333000000000007;
 * rtagsel = 0xffff0000ffffffff;
 *
 * // MPI_Irecv source_rank=3, tag=MPI_ANY_TAG, comm=MPI_COMM_WORLD
 * rtag    = 0x3333000300000000;
 * rtagsel = 0xffffffff80000000; // can't ignore sign bit in tag
 *
 * // MPI_Irecv source_rank=MPI_ANY_SOURCE, tag=MPI_ANY_TAG, comm=MPI_COMM_WORLD
 * rtag    = 0x3333000300000000;
 * rtagsel = 0xffff000080000000; // can't ignore sign bit in tag
 * @endverbatim
 *
 *
 * Applications that do not follow tag matching semantics can simply always
 * pass a value of @c 0 for @c rtagsel, which will always yield a successful
 * match to the first preposted buffer.  If a message cannot be matched to any
 * of the preposted buffers, the message is delivered as an unexpected
 * message.
 *
 * @section mq_receive MQ Message Reception
 *
 * MQ messages are either received as @e expected or @e unexpected: @li The
 * received message is @e expected if the incoming message tag matches the
 * combination of tag and tag selector of at least one of the user-provided
 * receive buffers preposted with @ref psm_mq_irecv.
 *
 * @li The received message is @e unexpected if the incoming message tag @b
 * doesn't match any combination of tag and tag selector from all the
 * user-provided receive buffers preposted with @ref psm_mq_irecv.
 *
 * Unexpected messages are messages that the MQ library buffers until the
 * user provides a receive buffer that can match the unexpected message.
 * With Matched Queues and MPI alike, unexpected messages can occur as a
 * side-effect of the programming model, whereby the arrival of messages can be
 * slightly out of step with the ordering in which the user
 * provides receive buffers.  Unexpected messages can also be triggered by the
 * difference between the rate at which a sender produces messages and the rate
 * at which a paired receiver can post buffers and hence consume the messages.
 *
 * In all cases, too many @e unexpected messages will negatively affect
 * performance.  Users can employ some of the following mechanisms to reduce
 * the effect of added memory allocations and copies that result from
 * unexpected messages:
 *   @li If and when possible, receive buffers should be posted as early as
 *       possible and ideally before calling into the progress engine.
 *   @li Use of rendezvous messaging that can be controlled with
 *       @ref PSM_MQ_RNDV_HFI_SZ and @ref PSM_MQ_RNDV_SHM_SZ options.  These
 *       options default to values determined to make effective use of
 *       bandwidth and are hence not advisable for all communication message
 *       sizes, but rendezvous messages inherently prevent unexpected
 *       messages by synchronizing the sender with the receiver beforehand.
 *   @li The amount of memory that is allocated to handle unexpected messages
 *       can be bounded by adjusting the Global @ref PSM_MQ_MAX_SYSBUF_MBYTES
 *       option.
 *   @li MQ statistics, such as the amount of received unexpected messages and
 *       the aggregate amount of unexpected bytes are available in the @ref
 *       psm_mq_stats structure.
 *
 * Whenever a match occurs, whether the message is expected or unexpected, it
 * is generally up to the user to ensure that the message is not truncated.
 * Message truncation occurs when the size of the preposted buffer is less than
 * the size of the incoming matched message.  MQ will correctly handle
 * message truncation by always copying the appropriate amount of bytes as to
 * not overwrite any data.  While it is valid to send less data than the amount
 * of data that has been preposted, messages that are truncated will be marked
 * @ref PSM_MQ_TRUNCATION as part of the error code in the message status
 * structure (@ref psm_mq_status_t or @ref psm_mq_status2_t).
 *
 * @section mq_completion MQ Completion Semantics
 *
 * Message completion in Matched Queues follows local completion semantics.
 * When sending an MQ message, it is deemed complete when MQ guarantees that
 * the source data has been sent and that the entire input source data memory
 * location can be safely overwritten.  As with standard Message-Passing,
 * MQ does not make any remote completion guarantees for sends.  MQ does
 * however, allow a sender to synchronize with a receiver to send a synchronous
 * message which sends a message only after a matching receive buffer has been
 * posted by the receiver (@ref PSM_MQ_FLAG_SENDSYNC).
 *
 * A receive is deemed complete after it has matched its associated receive
 * buffer with an incoming send and that the data from the send has been
 * completely delivered to the receive buffer.
 *
 * @section mq_progress MQ Progress Requirements
 *
 * Progress on MQs must be @e explicitly ensured by the user for correctness.
 * The progress requirement holds even if certain areas of the MQ
 * implementation require less network attention than others, or if progress
 * may internally be guaranteed through interrupts.  The main polling function,
 * @ref psm_poll, is the most general form of ensuring process on a given
 * endpoint.  Calling @ref psm_poll ensures that progress is made over all the
 * MQs and other components instantiated over the endpoint passed to @ref
 * psm_poll.
 *
 * While @ref psm_poll is the only way to directly ensure progress, other MQ
 * functions will conditionally ensure progres depending on how they are used:
 *
 * @li @ref psm_mq_wait employs polling and waits until the request is
 * completed.  For blocking communication operations where the caller is
 * waiting on a single send or receive to complete, psm_mq_wait usually
 * provides the best responsiveness in terms of latency.
 *
 * @li @ref psm_mq_test can test a particular request for completion, but @b
 * never directly or indirectly ensures progress as it only tests the
 * completion status of a request, nothing more.  See functional documentation
 * in @ref psm_mq_test for a detailed discussion.
 *
 * @li @ref psm_mq_ipeek ensures progress if and only if the MQ's completion
 * queue is empty and will not ensure progress as long as the completion queue
 * is non-empty.  Users that always aggressively process all elements of the MQ
 * completion queue as part of their own progress engine will indirectly always
 * ensure MQ progress. The ipeek mechanism is the preferred way for
 * ensuring progress when many non-blocking requests are in flight since ipeek
 * returns requests in the order in which they complete.  Depending on how the
 * user initiates and completes communication, this may be preferable to
 * calling other progress functions on individual requests.
 */

/*! @defgroup mq PSM Matched Queues
 *
 * @{
 */

/** @brief Initialize the MQ component for MQ communication
 *
 * This function provides the Matched Queue handle necessary to performa all
 * Matched Queue communication operations.
 *
 * @param[in] ep Endpoint over which to initialize Matched Queue
 * @param[in] tag_order_mask Order mask hint to let MQ know what bits of the
 *                           send tag are required to maintain MQ message
 *                           order.  In MPI parlance, this mask sets the bits
 *                           that store the context (or communicator ID).  The
 *                           user can choose to pass PSM_MQ_ORDERMASK_NONE or
 *                           PSM_MQ_ORDERMASK_ALL to tell MQ to respectively
 *                           provide no ordering guarantees or to provide
 *                           ordering over all messages by ignoring the
 *                           contexts of the send tags.
 * @param[in] opts Set of options for Matched Queue
 * @param[in] numopts Number of options passed
 * @param[out] mq User-supplied storage to return the Matched Queue handle
 *                associated to the newly created Matched Queue.
 *
 * @remark This function can be called many times to retrieve the MQ handle
 *         associated to an endpoint, but options are only considered the first
 *         time the function is called.
 *
 * @post The user obtains a handle to an instantiated Match Queue.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK A new Matched Queue has been instantiated across all the
 *         members of the group.
 *
 * @verbatim
 * int try_open_endpoint_and_initialize_mq(
 *	  psm_ep_t *ep,	// endpoint handle
 *	  psm_epid_t *epid, // unique endpoint ID
 *	  psm_uuid_t job_uuid, // unique job uuid, for ep_open
 *	  psm_mq_t *mq, // MQ handle initialized on endpoint 'ep'
 *        uint64_t communicator_bits) // Where we store our communicator or
 *                                    // context bits in the 64-bit tag.
 * {
 *     // Simplifed open, see psm_ep_open documentation for more info
 *     psm_ep_open(job_uuid,
 *                 NULL, // no options
 *                 ep, epid);
 *
 *     // We initialize a matched queue by telling PSM the bits that are
 *     // order-significant in the tag.  Point-to-point ordering will not be
 *     // maintained between senders where the communicator bits are not the
 *     // same.
 *     psm_mq_init(ep,
 *                 communicator_bits,
 *                 NULL, // no other MQ options
 *                 0,    // 0 options passed
 *                 mq);  // newly initialized matched Queue
 *
 *     return 1;
 * }
 * @endverbatim
 */
psm_error_t
psm_mq_init(psm_ep_t ep, uint64_t tag_order_mask,
	    const struct psm_optkey *opts, int numopts, psm_mq_t *mq);

#define PSM_MQ_ORDERMASK_NONE	0ULL
	/**< Used to initialize MQ and disable all MQ message ordering
	 * guarantees (this mask may prevent the use of MQ to maintain matched
	 * message envelope delivery required in MPI). */

#define PSM_MQ_ORDERMASK_ALL	0xffffffffffffffffULL
	/**< Used to initialize MQ with no message ordering hints, which forces
	 * MQ to maintain order over all messages */

/** @brief Finalize (close) an MQ handle
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK A given Matched Queue has been freed and use of the future
 * use of the handle produces undefined results.
 */
	 psm_error_t psm_mq_finalize(psm_mq_t mq);

#define PSM_MQ_TAG_ELEMENTS 3
	/**< Represents the number of 32-bit tag elements in the psm_mq_tag_t
	 *   type. */

/** @brief MQ Message tag
 *
 * Extended message tag type introduced in PSM 2.0.  The previous 64 bit tag
 * values are replaced by a struct containing three 32 bit tag values for a
 * total of 96 bits.  Matching semantics are unchanged from the previous 64-bit
 * matching scheme; the only difference is that 96 bits are matched instead of
 * 64.  For interoperability with existing PSM routines, 64 bit tags are
 * extended to a 96 bit tag by setting the upper 32 bits (tag[2] or tag2) to
 * zero.  Other than this caveat, all of the existing routines using 64-bit
 * tags are interchangeable with PSM2 routines using this psm_mq_tag_t type.
 * For example, a message sent using @ref psm_mq_send can be received using
 * @ref psm_mq_irecv2, provided the tags match as described above.
 */
typedef
struct psm_mq_tag {
	union {
		uint32_t tag[PSM_MQ_TAG_ELEMENTS] __attribute__ ((aligned(16)));
		struct {
			uint32_t tag0;
			uint32_t tag1;
			uint32_t tag2;
		};
	};
} psm_mq_tag_t;

/** @brief MQ Non-blocking operation status
 *
 * Message completion status for asynchronous communication operations.
 * For wait and test functions, MQ fills in the structure upon completion.
 * Upon completion, receive requests fill in every field of the status
 * structure while send requests only return a valid error_code and context
 * pointer.
 */
typedef
struct psm_mq_status {
	/** Sender's original message tag (receive reqs only) */
	uint64_t msg_tag;
	/** Sender's original message length (receive reqs only) */
	uint32_t msg_length;
	/** Actual number of bytes transfered (receive reqs only) */
	uint32_t nbytes;
	/** MQ error code for communication operation */
	psm_error_t error_code;
	/**< User-associated context for send or receive */
	void *context;
} psm_mq_status_t;

/** @brief MQ Non-blocking operation status
 *
 * Message completion status for asynchronous communication operations.  For
 * wait and test functions, MQ fills in the structure upon completion.  Upon
 * completion, requests fill in every field of the status structure with the
 * exception of the nbytes field, which is only valid for receives.  Version 2
 * of the status type contains an @ref psm_mq_tag_t type to represent the tag
 * instead of a 64-bit integer value and is for use with PSM v2 routines.
 */

typedef
struct psm_mq_status2 {
	/** Remote peer's epaddr */
	psm_epaddr_t msg_peer;
	/** Sender's original message tag */
	psm_mq_tag_t msg_tag;
	/** Sender's original message length */
	uint32_t msg_length;
	/** Actual number of bytes transfered (receiver only) */
	uint32_t nbytes;
	/** MQ error code for communication operation */
	psm_error_t error_code;
	/** User-associated context for send or receive */
	void *context;
} psm_mq_status2_t;

/** @brief PSM Communication handle (opaque) */
typedef struct psm_mq_req *psm_mq_req_t;

/*! @} */
/*! @ingroup mq
 * @defgroup mq_options PSM Matched Queue Options
 * @{
 *
 * MQ options can be modified at any point at runtime, unless otherwise noted.
 * The following example shows how to retrieve the current message size at
 * which messages are sent as synchronous.
 *
 * @verbatim
 * uint32_t get_hfirv_size(psm_mq_t mq)
 * {
 *     uint32_t rvsize;
 *     psm_getopt(mq, PSM_MQ_RNDV_HFI_SZ, &rvsize);
 *     return rvsize;
 * }
 * @endverbatim
 */

/** @brief Get an MQ option (Deprecated. Use psm_getopt with PSM_COMPONENT_MQ)
 *
 * Function to retrieve the value of an MQ option.
 *
 * @param[in] mq Matched Queue handle
 * @param[in] option Index of option to retrieve.  Possible values are:
 *            @li @ref PSM_MQ_RNDV_HFI_SZ
 *            @li @ref PSM_MQ_RNDV_SHM_SZ
 *            @li @ref PSM_MQ_MAX_SYSBUF_MBYTES
 *
 * @param[in] value Pointer to storage that can be used to store the value of
 *            the option to be set.  It is up to the user to ensure that the
 *            pointer points to a memory location large enough to accomodate
 *            the value associated to the type.  Each option documents the size
 *            associated to its value.
 *
 * @returns PSM_OK if option could be retrieved.
 * @returns PSM_PARAM_ERR if the option is not a valid option number
 */
psm_error_t psm_mq_getopt(psm_mq_t mq, int option, void *value);

/** @brief Set an MQ option (Deprecated. Use psm_setopt with PSM_COMPONENT_MQ)
 *
 * Function to set the value of an MQ option.
 *
 * @param[in] mq Matched Queue handle
 * @param[in] option Index of option to retrieve.  Possible values are:
 *            @li @ref PSM_MQ_RNDV_HFI_SZ
 *            @li @ref PSM_MQ_RNDV_SHM_SZ
 *            @li @ref PSM_MQ_MAX_SYSBUF_MBYTES
 *
 * @param[in] value Pointer to storage that contains the value to be updated
 *                  for the supplied option number.  It is up to the user to
 *                  ensure that the pointer points to a memory location with a
 *                  correct size.
 *
 * @returns PSM_OK if option could be retrieved.
 * @returns PSM_PARAM_ERR if the option is not a valid option number
 * @returns PSM_OPT_READONLY if the option to be set is a read-only option
 *                           (currently no MQ options are read-only).
 */
psm_error_t psm_mq_setopt(psm_mq_t mq, int option, const void *value);

/*! @}  */
/*! @ingroup mq
 * @{
 */

#define PSM_MQ_FLAG_SENDSYNC	0x01
				/**< MQ Send Force synchronous send */

#define PSM_MQ_REQINVALID	((psm_mq_req_t)(NULL))
				/**< MQ request completion value */

#define PSM_MQ_ANY_ADDR		((psm_epaddr_t)NULL)
				/**< MQ receive from any source epaddr */

/** @brief Post a receive to a Matched Queue with tag selection criteria
 *
 * Function to receive a non-blocking MQ message by providing a preposted
 * buffer. For every MQ message received on a particular MQ, the @c tag and @c
 * tagsel parameters are used against the incoming message's send tag as
 * described in @ref tagmatch.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] rtag Receive tag
 * @param[in] rtagsel Receive tag selector
 * @param[in] flags Receive flags (None currently supported)
 * @param[in] buf Receive buffer
 * @param[in] len Receive buffer length
 * @param[in] context User context pointer, available in @ref psm_mq_status_t
 *                    upon completion
 * @param[out] req PSM MQ Request handle created by the preposted receive, to
 *                 be used for explicitly controlling message receive
 *                 completion.
 *
 * @post The supplied receive buffer is given to MQ to match against incoming
 *       messages unless it is cancelled via @ref psm_mq_cancel @e before any
 *       match occurs.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The receive buffer has successfully been posted to the MQ.
 */
psm_error_t
psm_mq_irecv(psm_mq_t mq, uint64_t rtag, uint64_t rtagsel, uint32_t flags,
	     void *buf, uint32_t len, void *context, psm_mq_req_t *req);

/** @brief Post a receive to a Matched Queue with source and tag selection
 *  criteria
 *
 * Function to receive a non-blocking MQ message by providing a preposted
 * buffer. For every MQ message received on a particular MQ, the @c src, @c tag
 * and @c tagsel parameters are used against the incoming message's send tag as
 * described in @ref tagmatch.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] src Source (sender's) epaddr (may be PSM_MQ_ANY_ADDR)
 * @param[in] rtag Receive tag
 * @param[in] rtagsel Receive tag selector
 * @param[in] flags Receive flags (None currently supported)
 * @param[in] buf Receive buffer
 * @param[in] len Receive buffer length
 * @param[in] context User context pointer, available in @ref psm_mq_status2_t
 *                    upon completion
 * @param[out] req PSM MQ Request handle created by the preposted receive, to
 *                 be used for explicitly controlling message receive
 *                 completion.
 *
 * @post The supplied receive buffer is given to MQ to match against incoming
 *       messages unless it is cancelled via @ref psm_mq_cancel @e before any
 *       match occurs.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The receive buffer has successfully been posted to the MQ.
 */
psm_error_t
psm_mq_irecv2(psm_mq_t mq, psm_epaddr_t src, psm_mq_tag_t *rtag,
	      psm_mq_tag_t *rtagsel, uint32_t flags, void *buf, uint32_t len,
	      void *context, psm_mq_req_t *req);

/** @brief Post a receive to a Matched Queue with matched request
 *
 * Function to receive a non-blocking MQ message by providing a preposted
 * buffer. The provided request should already be matched using the @ref
 * psm_mq_improbe or @ref psm_mq_improbe2 routines.  It is an error to pass a
 * request that has not already been matched by one of those routines.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] flags Receive flags (None currently supported)
 * @param[in] buf Receive buffer
 * @param[in] len Receive buffer length
 * @param[in] context User context pointer, available in @ref psm_mq_status_t
 *                    upon completion
 * @param[inout] req PSM MQ Request handle matched previously by a matched
 *		     probe routine (@ref psm_mq_improbe or @ref
 *		     psm_mq_improbe2), also to be used for explicitly
 *		     controlling message receive completion.
 *
 * @post The supplied receive buffer is given to MQ to deliver the matched
 *       message.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The receive buffer has successfully been posted to the MQ.
 */
psm_error_t
psm_mq_imrecv(psm_mq_t mq, uint32_t flags, void *buf, uint32_t len,
	      void *context, psm_mq_req_t *reqo);

/** @brief Send a blocking MQ message
 *
 * Function to send a blocking MQ message, whereby the message is locally
 * complete and the source data can be modified upon return.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] dest Destination EP address
 * @param[in] flags Message flags, currently:
 *            @li PSM_MQ_FLAG_SENDSYNC tells PSM to send the message
 *            synchronously, meaning that the message will not be sent until
 *            the receiver acknowledges that it has matched the send with a
 *            receive buffer.
 * @param[in] stag Message Send Tag
 * @param[in] buf Source buffer pointer
 * @param[in] len Length of message starting at @c buf.
 *
 * @post The source buffer is reusable and the send is locally complete.
 *
 * @note This send function has been implemented to best suit MPI_Send.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The message has been successfully sent.
 */
psm_error_t
psm_mq_send(psm_mq_t mq, psm_epaddr_t dest, uint32_t flags, uint64_t stag,
	    const void *buf, uint32_t len);

/** @brief Send a blocking MQ message
 *
 * Function to send a blocking MQ message, whereby the message is locally
 * complete and the source data can be modified upon return.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] dest Destination EP address
 * @param[in] flags Message flags, currently:
 *            @li PSM_MQ_FLAG_SENDSYNC tells PSM to send the message
 *            synchronously, meaning that the message will not be sent until
 *            the receiver acknowledges that it has matched the send with a
 *            receive buffer.
 * @param[in] stag Message Send Tag
 * @param[in] buf Source buffer pointer
 * @param[in] len Length of message starting at @c buf.
 *
 * @post The source buffer is reusable and the send is locally complete.
 *
 * @note This send function has been implemented to best suit MPI_Send.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The message has been successfully sent.
 */
psm_error_t
psm_mq_send2(psm_mq_t mq, psm_epaddr_t dest, uint32_t flags,
	     psm_mq_tag_t *stag, const void *buf, uint32_t len);

/** @brief Send a non-blocking MQ message
 *
 * Function to initiate the send of a non-blocking MQ message, whereby the
 * user guarantees that the source data will remain unmodified until the send
 * is locally completed through a call such as @ref psm_mq_wait or @ref
 * psm_mq_test.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] dest Destination EP address
 * @param[in] flags Message flags, currently:
 *            @li PSM_MQ_FLAG_SENDSYNC tells PSM to send the message
 *            synchronously, meaning that the message will not be sent until
 *            the receiver acknowledges that it has matched the send with a
 *            receive buffer.
 * @param[in] stag Message Send Tag
 * @param[in] buf Source buffer pointer
 * @param[in] len Length of message starting at @c buf.
 * @param[in] context Optional user-provided pointer available in @ref
 *                    psm_mq_status_t when the send is locally completed.
 * @param[out] req PSM MQ Request handle created by the non-blocking send, to
 *                 be used for explicitly controlling message completion.
 *
 * @post The source buffer is not reusable and the send is not locally complete
 *       until its request is completed by either @ref psm_mq_test or @ref
 *       psm_mq_wait.
 *
 * @note This send function has been implemented to suit MPI_Isend.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The message has been successfully initiated.
 *
 * @verbatim
 * psm_mq_req_t
 * non_blocking_send(const psm_mq_t mq, psm_epaddr_t dest_ep,
 *                       const void *buf, uint32_t len,
 *			 int context_id, int send_tag, const my_request_t *req)
 * {
 *     psm_mq_req_t req_mq;
 *     // Set up our send tag, assume that "my_rank" is global and represents
 *     // the rank of this process in the job
 *     uint64_t tag = ( ((context_id & 0xffff) << 48) |
 *                      ((my_rank & 0xffff) << 32)    |
 *                      ((send_tag & 0xffffffff)) );
 *
 *     psm_mq_isend(mq, dest_ep,
 *                  0, // no flags
 *                  tag,
 *                  buf,
 *                  len,
 *                  req, // this req is available in psm_mq_status_t when one
 *                       // of the synchronization functions is called.
 *                  &req_mq);
 *     return req_mq;
 * }
 * @endverbatim
 */
psm_error_t
psm_mq_isend(psm_mq_t mq, psm_epaddr_t dest, uint32_t flags, uint64_t stag,
	     const void *buf, uint32_t len, void *context, psm_mq_req_t *req);

/** @brief Send a non-blocking MQ message
 *
 * Function to initiate the send of a non-blocking MQ message, whereby the
 * user guarantees that the source data will remain unmodified until the send
 * is locally completed through a call such as @ref psm_mq_wait or @ref
 * psm_mq_test.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] dest Destination EP address
 * @param[in] flags Message flags, currently:
 *            @li PSM_MQ_FLAG_SENDSYNC tells PSM to send the message
 *            synchronously, meaning that the message will not be sent until
 *            the receiver acknowledges that it has matched the send with a
 *            receive buffer.
 * @param[in] stag Message Send Tag, array of three 32-bit values.
 * @param[in] buf Source buffer pointer
 * @param[in] len Length of message starting at @c buf.
 * @param[in] context Optional user-provided pointer available in @ref
 *                    psm_mq_status2_t when the send is locally completed.
 * @param[out] req PSM MQ Request handle created by the non-blocking send, to
 *                 be used for explicitly controlling message completion.
 *
 * @post The source buffer is not reusable and the send is not locally complete
 *       until its request is completed by either @ref psm_mq_test or @ref
 *       psm_mq_wait.
 *
 * @note This send function has been implemented to suit MPI_Isend.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The message has been successfully initiated.
 *
 * @verbatim
 * psm_mq_req_t
 * non_blocking_send(const psm_mq_t mq, psm_epaddr_t dest_ep,
 *                       const void *buf, uint32_t len,
 *			 int context_id, int send_tag, const my_request_t *req)
 * {
 *     psm_mq_req_t req_mq;
 *     // Set up our send tag, assume that "my_rank" is global and represents
 *     // the rank of this process in the job
 *     psm_mq_tag_t tag;
 *     tag.tag[0] = send_tag;
 *     tag.tag[1] = my_rank;
 *     tag.tag[2] = context_id;
 *
 *     psm_mq_isend(mq, dest_ep,
 *                  0, // no flags
 *                  &tag,
 *                  buf,
 *                  len,
 *                  req, // this req is available in psm_mq_status2_t when one
 *                       // of the synchronization functions is called.
 *                  &req_mq);
 *     return req_mq;
 * }
 * @endverbatim
 */
psm_error_t
psm_mq_isend2(psm_mq_t mq, psm_epaddr_t dest, uint32_t flags,
	      psm_mq_tag_t *stag, const void *buf, uint32_t len, void *context,
	      psm_mq_req_t *req);

/** @brief Try to Probe if a message is received matching tag selection
 * criteria
 *
 * Function to verify if a message matching the supplied tag and tag selectors
 * has been received.  The message is not fully matched until the user
 * provides a buffer with the successfully matching tag selection criteria
 * through @ref psm_mq_irecv.
 * Probing for messages may be useful if the size of the
 * message to be received is unknown, in which case its size will be
 * available in the @c msg_length member of the returned @c status.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] rtag Message receive tag
 * @param[in] rtagsel Message receive tag selector
 * @param[out] status Upon return, @c status is filled with information
 *                    regarding the matching send.
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The iprobe is successful and status is updated if non-NULL.
 * @retval PSM_MQ_NO_COMPLETIONS The iprobe is unsuccessful and status is
 *                               unchaged.
 */
psm_error_t
psm_mq_iprobe(psm_mq_t mq, uint64_t rtag, uint64_t rtagsel,
	      psm_mq_status_t *status);

/** @brief Try to Probe if a message is received matching source and tag
 * selection criteria
 *
 * Function to verify if a message matching the supplied source, tag, and tag
 * selectors has been received.  The message is not fully matched until the
 * user provides a buffer with the successfully matching tag selection criteria
 * through @ref psm_mq_irecv.  Probing for messages may be useful if the size
 * of the message to be received is unknown, in which case its size will be
 * available in the @c msg_length member of the returned @c status.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] src Source (sender's) epaddr (may be PSM_MQ_ANY_ADDR)
 * @param[in] rtag Message receive tag
 * @param[in] rtagsel Message receive tag selector
 * @param[out] status Upon return, @c status is filled with information
 *                    regarding the matching send.
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The iprobe is successful and status is updated if non-NULL.
 * @retval PSM_MQ_NO_COMPLETIONS The iprobe is unsuccessful and status is
 *                               unchaged.
 */
psm_error_t
psm_mq_iprobe2(psm_mq_t mq, psm_epaddr_t src, psm_mq_tag_t *rtag,
	       psm_mq_tag_t *rtagsel, psm_mq_status2_t *status);

/** @brief Try to Probe if a message is received matching tag selection
 * criteria
 *
 * Function to verify if a message matching the supplied source, tag, and tag
 * selectors has been received.  If a match is successful, the message is
 * removed from the matching queue and returned as a request object.  The
 * message can be received using @ref psm_mq_imrecv.  It is erroneous to use
 * the request object returned by @ref psm_mq_improbe for any purpose other
 * than passing to @ref psm_mq_imrecv.  Probing for messages may be useful if
 * the size of the message to be received is unknown, in which case its size
 * will be available in the @c msg_length member of the returned @c status.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] rtag Message receive tag
 * @param[in] rtagsel Message receive tag selector
 * @param[out] req PSM MQ Request handle, to be used for receiving the matched
 *                 message.
 * @param[out] status Upon return, @c status is filled with information
 *                    regarding the matching send.
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The iprobe is successful and status is updated if non-NULL.
 * @retval PSM_MQ_NO_COMPLETIONS The iprobe is unsuccessful and status is unchaged.
 */
psm_error_t
psm_mq_improbe(psm_mq_t mq, uint64_t rtag, uint64_t rtagsel, psm_mq_req_t *req,
	       psm_mq_status_t *status);

/** @brief Try to Probe if a message is received matching source and tag
 * selection criteria
 *
 * Function to verify if a message matching the supplied tag and tag selectors
 * has been received.  If a match is successful, the message is removed from
 * the matching queue and returned as a request object.  The message can be
 * received using @ref psm_mq_imrecv.  It is erroneous to use the request
 * object returned by @ref psm_mq_improbe for any purpose other than passing to
 * @ref psm_mq_imrecv.  Probing for messages may be useful if the size of the
 * message to be received is unknown, in which case its size will be available
 * in the @c msg_length member of the returned @c status.
 *
 * @param[in] mq Matched Queue Handle
 * @param[in] src Source (sender's) epaddr (may be PSM_MQ_ANY_ADDR)
 * @param[in] rtag Message receive tag
 * @param[in] rtagsel Message receive tag selector
 * @param[out] status Upon return, @c status is filled with information
 *                    regarding the matching send.
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The iprobe is successful and status is updated if non-NULL.
 * @retval PSM_MQ_NO_COMPLETIONS The iprobe is unsuccessful and status is unchaged.
 */
psm_error_t
psm_mq_improbe2(psm_mq_t mq, psm_epaddr_t src, psm_mq_tag_t *rtag,
		psm_mq_tag_t *rtagsel, psm_mq_req_t *req,
		psm_mq_status2_t *status);

/** @brief Query for non-blocking requests ready for completion.
 *
 * Function to query a particular MQ for non-blocking requests that are ready
 * for completion.  Requests "ready for completion" are not actually considered
 * complete by MQ until they are returned to the MQ library through @ref
 * psm_mq_wait or @ref psm_mq_test.
 *
 * If the user can deal with consuming request completions in the order in
 * which they complete, this function can be used both for completions and for
 * ensuring progress.  The latter requirement is satisfied when the user
 * peeks an empty completion queue as a side effect of always aggressively
 * peeking and completing all an MQ's requests ready for completion.
 *
 *
 * @param[in] mq Matched Queue Handle
 * @param[in,out] req MQ non-blocking request
 * @param[in] status Optional MQ status, can be NULL.
 *
 * @post The user has ensured progress if the function returns @ref
 *       PSM_MQ_NO_COMPLETIONS
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The peek is successful and @c req is updated with a request
 *                ready for completion.  If @c status is non-NULL, it is also
 *                updated.
 *
 * @retval PSM_MQ_NO_COMPLETIONS The peek is not successful, meaning that there
 *                               are no further requests ready for completion.
 *                               The contents of @c req and @c status remain
 *                               unchanged.
 * @verbatim
 * // Example that uses ipeek_mq_ipeek to make progress instead of psm_poll
 * // We return the amount of non-blocking requests that we've completed
 * int main_progress_loop(psm_mq_t mq)
 * {
 *     int num_completed = 0;
 *     psm_mq_req_t req;
 *     psm_mq_status_t status;
 *     psm_error_t err;
 *     my_request_t *myreq;
 *
 *     do {
 *         err = psm_mq_ipeek(mq, &req,
 *                            NULL); // No need for status in ipeek here
 *         if (err == PSM_MQ_NO_COMPLETIONS)
 *             return num_completed;
 *         else if (err != PSM_OK)
 *             goto errh;
 *         num_completed++;
 *
 *         // We obtained 'req' at the head of the completion queue.  We can
 *         // now free the request with PSM and obtain our original reques
 *         // from the status' context
 *         err = psm_mq_test(&req, // will be marked as invalid
 *                           &status); // we need the status
 *         myreq = (my_request_t *) status.context;
 *
 *         // handle the completion for myreq whether myreq is a posted receive
 *         // or a non-blocking send.
 *    }
 *    while (1);
 * }
 * @endverbatim */
psm_error_t
psm_mq_ipeek(psm_mq_t mq, psm_mq_req_t *req, psm_mq_status_t *status);

/** @brief Query for non-blocking requests ready for completion.
 *
 * Function to query a particular MQ for non-blocking requests that are ready
 * for completion.  Requests "ready for completion" are not actually considered
 * complete by MQ until they are returned to the MQ library through @ref
 * psm_mq_wait or @ref psm_mq_test.
 *
 * If the user can deal with consuming request completions in the order in
 * which they complete, this function can be used both for completions and for
 * ensuring progress.  The latter requirement is satisfied when the user
 * peeks an empty completion queue as a side effect of always aggressively
 * peeking and completing all an MQ's requests ready for completion.
 *
 *
 * @param[in] mq Matched Queue Handle
 * @param[in,out] req MQ non-blocking request
 * @param[in] status Optional MQ status, can be NULL.
 *
 * @post The user has ensured progress if the function returns @ref
 *       PSM_MQ_NO_COMPLETIONS
 *
 * The following error codes are returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The peek is successful and @c req is updated with a request
 *                ready for completion.  If @c status is non-NULL, it is also
 *                updated.
 *
 * @retval PSM_MQ_NO_COMPLETIONS The peek is not successful, meaning that there
 *                            are no further requests ready for completion.
 *                            The contents of @c req and @c status remain
 *                            unchanged.
 * @verbatim
 * // Example that uses ipeek_mq_ipeek to make progress instead of psm_poll
 * // We return the amount of non-blocking requests that we've completed
 * int main_progress_loop(psm_mq_t mq)
 * {
 *     int num_completed = 0;
 *     psm_mq_req_t req;
 *     psm_mq_status2_t status;
 *     psm_error_t err;
 *     my_request_t *myreq;
 *
 *     do {
 *         err = psm_mq_ipeek2(mq, &req,
 *                            NULL); // No need for status in ipeek here
 *         if (err == PSM_MQ_NO_COMPLETIONS)
 *             return num_completed;
 *         else if (err != PSM_OK)
 *             goto errh;
 *         num_completed++;
 *
 *         // We obtained 'req' at the head of the completion queue.  We can
 *         // now free the request with PSM and obtain our original reques
 *         // from the status' context
 *         err = psm_mq_test2(&req, // will be marked as invalid
 *                           &status); // we need the status
 *         myreq = (my_request_t *) status.context;
 *
 *         // handle the completion for myreq whether myreq is a posted receive
 *         // or a non-blocking send.
 *    }
 *    while (1);
 * }
 * @endverbatim */
psm_error_t
psm_mq_ipeek2(psm_mq_t mq, psm_mq_req_t *req, psm_mq_status2_t *status);

/** @brief Wait until a non-blocking request completes
 *
 * Function to wait on requests created from either preposted receive buffers
 * or non-blocking sends.  This is the only blocking function in the MQ
 * interface and will poll until the request is complete as per the progress
 * semantics explained in @ref mq_progress.
 *
 * @param[in,out] request MQ non-blocking request
 * @param[out] status Updated if non-NULL when request successfully completes
 *
 * @pre The user has obtained a valid MQ request by calling @ref psm_mq_isend
 *      or @ref psm_mq_irecv and passes a pointer to enough storage to write
 *      the output of a @ref psm_mq_status_t or NULL if status is to be
 *      ignored.
 *
 * @pre Since MQ will internally ensure progress while the user is
 *      suspended, the user need not ensure that progress is made prior to
 *      calling this function.
 *
 * @post The request is assigned the value @ref PSM_MQ_REQINVALID and all
 *       associated MQ request storage is released back to the MQ library.
 *
 * @remarks
 *  @li This function ensures progress on the endpoint as long as the request
 *      is incomplete.
 *  @li @c status can be NULL, in which case no status is written upon
 *      completion.
 *  @li If @c request is @ref PSM_MQ_REQINVALID, the function returns
 *      immediately.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The request is complete or the value of @c was
 *                @ref PSM_MQ_REQINVALID.
 *
 */
psm_error_t
psm_mq_wait(psm_mq_req_t *request, psm_mq_status_t *status);

/** @brief Wait until a non-blocking request completes
 *
 * Function to wait on requests created from either preposted receive buffers
 * or non-blocking sends.  This is the only blocking function in the MQ
 * interface and will poll until the request is complete as per the progress
 * semantics explained in @ref mq_progress.
 *
 * @param[in,out] request MQ non-blocking request
 * @param[out] status Updated if non-NULL when request successfully completes
 *
 * @pre The user has obtained a valid MQ request by calling @ref psm_mq_isend
 *      or @ref psm_mq_irecv and passes a pointer to enough storage to write
 *      the output of a @ref psm_mq_status2_t or NULL if status is to be
 *      ignored.
 *
 * @pre Since MQ will internally ensure progress while the user is
 *      suspended, the user need not ensure that progress is made prior to
 *      calling this function.
 *
 * @post The request is assigned the value @ref PSM_MQ_REQINVALID and all
 *       associated MQ request storage is released back to the MQ library.
 *
 * @remarks
 *  @li This function ensures progress on the endpoint as long as the request
 *      is incomplete.
 *  @li @c status can be NULL, in which case no status is written upon
 *      completion.
 *  @li If @c request is @ref PSM_MQ_REQINVALID, the function returns
 *      immediately.
 *
 * The following error code is returned.  Other errors are handled by the PSM
 * error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The request is complete or the value of @c was
 *                @ref PSM_MQ_REQINVALID.
 *
 */
psm_error_t
psm_mq_wait2(psm_mq_req_t *request, psm_mq_status2_t *status);

/** @brief Test if a non-blocking request is complete
 *
 * Function to test requests created from either preposted receive buffers or
 * non-blocking sends for completion.  Unlike @ref psm_mq_wait, this function
 * tests @c request for completion and @e never ensures progress directly or
 * indirectly.  It is up to the user to employ some of the progress functions
 * described in @ref mq_progress to ensure progress if the user chooses to
 * exclusively test requests for completion.
 *
 * Testing a request for completion @e never internally ensure progress in
 * order to be useful to construct higher-level completion tests over arrays to
 * test some, all or any request that has completed.  For testing arrays of
 * requests, it is preferable for performance reasons to only ensure progress
 * once before testing a set of requests for completion.
 *
 * @param[in,out] request MQ non-blocking request
 * @param[out] status Updated if non-NULL and the request successfully
 * completes
 *
 * @pre The user has obtained a valid MQ request by calling @ref psm_mq_isend
 *      or @ref psm_mq_irecv and passes a pointer to enough storage to write
 *      the output of a @ref psm_mq_status_t or NULL if status is to be
 *      ignored.
 *
 * @pre The user has ensured progress on the Matched Queue if @ref
 *      psm_mq_test is exclusively used for guaranteeing request completions.
 *
 * @post If the request is complete, the request is assigned the value @ref
 *       PSM_MQ_REQINVALID and all associated MQ request storage is released
 *       back to the MQ library. If the request is incomplete, the contents of
 *       @c request is unchanged.
 *
 * @post The user will ensure progress on the Matched Queue if @ref
 *       psm_mq_test is exclusively used for guaranteeing request completions.
 *
 * The following two errors are always returned.  Other errors are handled by
 * the PSM error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The request is complete and @c request is set to @ref
 *                PSM_MQ_REQINVALID or the value of @c was PSM_MQ_REQINVALID
 *
 * @retval PSM_MQ_NO_COMPLETIONS The request is not complete and @c request is
 *                           unchanged.
 *
 * @verbatim
 * // Function that returns the first completed request in an array
 * // of requests.
 * void *
 * user_testany(psm_mq_t mq, psm_mq_req_t *allreqs, int nreqs)
 * {
 *   int i;
 *   void *context = NULL;
 *
 *   // Ensure progress only once
 *   psm_poll(mq);
 *
 *   // Test for at least one completion and return it's context
 *   psm_mq_status_t stat;
 *   for (i = 0; i < nreqs; i++) {
 *     if (psm_mq_test(&allreqs[i], &stat) == PSM_OK) {
 *       context = stat.context;
 *       break;
 *     }
 *   }
 *   return context;
 * }
 * @endverbatim
 */
psm_error_t
psm_mq_test(psm_mq_req_t *request, psm_mq_status_t *status);

/** @brief Test if a non-blocking request is complete
 *
 * Function to test requests created from either preposted receive buffers or
 * non-blocking sends for completion.  Unlike @ref psm_mq_wait, this function
 * tests @c request for completion and @e never ensures progress directly or
 * indirectly.  It is up to the user to employ some of the progress functions
 * described in @ref mq_progress to ensure progress if the user chooses to
 * exclusively test requests for completion.
 *
 * Testing a request for completion @e never internally ensure progress in
 * order to be useful to construct higher-level completion tests over arrays to
 * test some, all or any request that has completed.  For testing arrays of
 * requests, it is preferable for performance reasons to only ensure progress
 * once before testing a set of requests for completion.
 *
 * @param[in,out] request MQ non-blocking request
 * @param[out] status Updated if non-NULL and the request successfully
 * completes
 *
 * @pre The user has obtained a valid MQ request by calling @ref psm_mq_isend
 *      or @ref psm_mq_irecv and passes a pointer to enough storage to write
 *      the output of a @ref psm_mq_status2_t or NULL if status is to be
 *      ignored.
 *
 * @pre The user has ensured progress on the Matched Queue if @ref
 *      psm_mq_test is exclusively used for guaranteeing request completions.
 *
 * @post If the request is complete, the request is assigned the value @ref
 *       PSM_MQ_REQINVALID and all associated MQ request storage is released
 *       back to the MQ library. If the request is incomplete, the contents of
 *       @c request is unchanged.
 *
 * @post The user will ensure progress on the Matched Queue if @ref
 *       psm_mq_test is exclusively used for guaranteeing request completions.
 *
 * The following two errors are always returned.  Other errors are handled by
 * the PSM error handler (@ref psm_error_register_handler).
 *
 * @retval PSM_OK The request is complete and @c request is set to @ref
 *                PSM_MQ_REQINVALID or the value of @c was PSM_MQ_REQINVALID
 *
 * @retval PSM_MQ_NO_COMPLETIONS The request is not complete and @c request is
 *                           unchanged.
 *
 * @verbatim
 * // Function that returns the first completed request in an array
 * // of requests.
 * void *
 * user_testany(psm_mq_t mq, psm_mq_req_t *allreqs, int nreqs)
 * {
 *   int i;
 *   void *context = NULL;
 *
 *   // Ensure progress only once
 *   psm_poll(mq);
 *
 *   // Test for at least one completion and return it's context
 *   psm_mq_status2_t stat;
 *   for (i = 0; i < nreqs; i++) {
 *     if (psm_mq_test2(&allreqs[i], &stat) == PSM_OK) {
 *       context = stat.context;
 *       break;
 *     }
 *   }
 *   return context;
 * }
 * @endverbatim
 */
psm_error_t
psm_mq_test2(psm_mq_req_t *request, psm_mq_status2_t *status);

/** @brief Cancel a preposted request
 *
 * Function to cancel a preposted receive request returned by @ref
 * psm_mq_irecv.  It is currently illegal to cancel a send request initiated
 * with @ref psm_mq_isend.
 *
 * @pre The user has obtained a valid MQ request by calling @ref psm_mq_isend.
 *
 * @post Whether the cancel is successful or not, the user returns the
 *       request to the library by way of @ref psm_mq_test or @ref
 *       psm_mq_wait.
 *
 * Only the two following errors can be returned directly, without being
 * handled by the error handler (@ref psm_error_register_handler):
 *
 * @retval PSM_OK The request could be successfully cancelled such that the
 *                preposted receive buffer could be removed from the preposted
 *                receive queue before a match occured. The associated @c
 *                request remains unchanged and the user must still return
 *                the storage to the MQ library.
 *
 * @retval PSM_MQ_NO_COMPLETIONS The request could not be successfully cancelled
 *                           since the preposted receive buffer has already
 *                           matched an incoming message.  The @c request
 *                           remains unchanged.
 *
 */
psm_error_t psm_mq_cancel(psm_mq_req_t *req);

/*! @brief MQ statistics structure */
struct psm_mq_stats {
	/** Bytes received into a matched user buffer */
	uint64_t rx_user_bytes;
	/** Messages received into a matched user buffer */
	uint64_t rx_user_num;
	/** Bytes received into an unmatched system buffer */
	uint64_t rx_sys_bytes;
	/** Messages received into an unmatched system buffer */
	uint64_t rx_sys_num;

	/** Total Messages transmitted (shm and hfi) */
	uint64_t tx_num;
	/** Messages transmitted eagerly */
	uint64_t tx_eager_num;
	/** Bytes transmitted eagerly */
	uint64_t tx_eager_bytes;
	/** Messages transmitted using expected TID mechanism */
	uint64_t tx_rndv_num;
	/** Bytes transmitted using expected TID mechanism */
	uint64_t tx_rndv_bytes;
	/** Messages transmitted (shm only) */
	uint64_t tx_shm_num;
	/** Messages received through shm */
	uint64_t rx_shm_num;

	/** Number of system buffers allocated  */
	uint64_t rx_sysbuf_num;
	/** Bytes allcoated for system buffers */
	uint64_t rx_sysbuf_bytes;

	/** Internally reserved for future use */
	uint64_t _reserved[16];
};

#define PSM_MQ_NUM_STATS    13	/**< How many stats are currently used in @ref psm_mq_stats */

/*! @see psm_mq_stats */
	typedef struct psm_mq_stats psm_mq_stats_t;

/** @brief Retrieve statistics from an instantied MQ */
	void
	 psm_mq_get_stats(psm_mq_t mq, psm_mq_stats_t *stats);

/*! @} */
#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif
