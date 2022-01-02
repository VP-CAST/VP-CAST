/*
Copyright (c) 1997, 1998 Carnegie Mellon University.  All Rights
Reserved. 

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The AODV code developed by the CMU/MONARCH group was optimized and tuned by Samir Das and Mahesh Marina, University of Cincinnati. The work was partially done in Sun Microsystems. Modified for gratuitous replies by Anant Utgikar, 09/16/02.

*/

//#include <ip.h>

#include <aodv/aodv.h>
#include <aodv/aodv_packet.h>
#include <random.h>
#include <cmu-trace.h>
//#include <energy-model.h>
//bjs begin
#include <mobilenode.h>
//bjs end
#define max(a,b)        ( (a) > (b) ? (a) : (b) )
#define CURRENT_TIME    Scheduler::instance().clock()

#define DEBUG
//#define ERROR
#define		min(x,y)	((x) < (y) ? (x) : (y))
#ifdef DEBUG
static int extra_route_reply = 0;
static int limit_route_request = 0;
static int route_request = 0;
#endif

int AODV::bid = 0;

/*
  TCL Hooks
*/


int hdr_aodv::offset_;
static class AODVHeaderClass : public PacketHeaderClass {
public:
        AODVHeaderClass() : PacketHeaderClass("PacketHeader/AODV",
                                              sizeof(hdr_all_aodv)) {
	  bind_offset(&hdr_aodv::offset_);
	} 
} class_rtProtoAODV_hdr;

static class AODVclass : public TclClass {
public:
        AODVclass() : TclClass("Agent/AODV") {}
        TclObject* create(int argc, const char*const* argv) {
          assert(argc == 5);
          //return (new AODV((nsaddr_t) atoi(argv[4])));
	  return (new AODV((nsaddr_t) Address::instance().str2addr(argv[4])));
        }
} class_rtProtoAODV;


int
AODV::command(int argc, const char*const* argv) {
  if(argc == 2) {
  Tcl& tcl = Tcl::instance();
    
    if(strncasecmp(argv[1], "id", 2) == 0) {
      tcl.resultf("%d", index);
      return TCL_OK;
    }
    
    if(strncasecmp(argv[1], "start", 2) == 0) {
        //printf("start call\n");
      btimer.handle((Event*) 0);

#ifndef AODV_LINK_LAYER_DETECTION
      htimer.handle((Event*) 0);
      ntimer.handle((Event*) 0);
      //mtimer.handle((Event*) 0);
#endif // LINK LAYER DETECTION

      rtimer.handle((Event*) 0);
      return TCL_OK;
    
    }             
    //bjs begin
    if(strncasecmp(argv[1], "b_start", 2) == 0) {
        //printf("b_start\n");
        source =index;
        sendMyHello();
		
        return TCL_OK;
    
    }
    if(strncasecmp(argv[1], "b_stop", 2) == 0) {
        //printf("b_stop\n");
        mtimer.b_stop();
        return TCL_OK;
    }
    if(strncasecmp(argv[1], "nb_show", 2) == 0) {
        //printf("\nmy id [%d] nb_show()\n", index);
        nb_show();
        return TCL_OK;
    }
    
    //bjs end
               
  }
  else if(argc == 3) {
    if(strcmp(argv[1], "index") == 0) {
      index = atoi(argv[2]);
      return TCL_OK;
    }

    else if(strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
      logtarget = (Trace*) TclObject::lookup(argv[2]);
      if(logtarget == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if(strcmp(argv[1], "drop-target") == 0) {
    int stat = rqueue.command(argc,argv);
      if (stat != TCL_OK) return stat;
      return Agent::command(argc, argv);
    }
    else if(strcmp(argv[1], "if-queue") == 0) {
    ifqueue = (PriQueue*) TclObject::lookup(argv[2]);
      
      if(ifqueue == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if (strcmp(argv[1], "port-dmux") == 0) {
    	dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
	if (dmux_ == 0) {
		fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__,
		argv[1], argv[2]);
		return TCL_ERROR;
	}
	return TCL_OK;
    }

    else if (strcmp(argv[1], "helloFlag_") == 0) {
    	helloFlag_ = atoi(argv[2]);
        htimer.hello_flag = helloFlag_;
	    //printf("hello flag : %d\n",helloFlag_);
	    return TCL_OK;
    }
    else if (strcmp(argv[1], "helloOnOff_") == 0) {
    	helloOnOff_ = atoi(argv[2]);
	    //printf("helloOnOff_ flag : %d\n",helloOnOff_);
	    return TCL_OK;
    }

  }
  return Agent::command(argc, argv);
}

/* 
   Constructor
*/

AODV::AODV(nsaddr_t id) : Agent(PT_AODV),
			  btimer(this), htimer(this), ntimer(this), 
			  rtimer(this), lrtimer(this),mtimer(this),mtimer2(this),rqueue(),wait_timer1_(this),wait_timer2_(this) {
 
                
  index = id;
  seqno = 2;
  bid = 1;
source = -1;
  LIST_INIT(&nbhead);
  LIST_INIT(&bihead);

  logtarget = 0;
  ifqueue = 0;
//bjs begin
  neighborflag_ = 0;
  helloFlag_ =0;
  helloOnOff_ = 1;
   // so that initially it should not represent any vehicle, if not initialized, it will by default represent vehicle 0, whether it is a source or not
//bjs end
}

/*
  Timers
*/

void
BroadcastTimer::handle(Event*) {
  agent->id_purge();
  Scheduler::instance().schedule(this, &intr, BCAST_ID_SAVE);
}

void
HelloTimer::handle(Event*) {
if(hello_flag){
   agent->sendHello();
   double interval = MinHelloInterval + 
                 ((MaxHelloInterval - MinHelloInterval) * Random::uniform());
   assert(interval >= 0);
   Scheduler::instance().schedule(this, &intr, interval);
    }
}

//bjs begin
void
MyTimer::handle(Event*) {
    //printf("Node[%d] MyTimer::handle(Event*)\n", agent->index);
   agent->sendMyHello();
    b_stop();
}
void
MyTimer::b_start(double waiting_time) {

	/*#ifdef DEBUG
      printf(" b_start is executed for %d\n",index);
#endif */ 
//   double interval = agent->hellointerval_;
    //printf("id[%d] b_start() calls\n",agent->index);
   double interval = waiting_time;
   assert(interval >= 0);
   Scheduler::instance().schedule(this, &intr, interval);
}

void
MyTimer::b_stop(void) {
        //printf("id[%d] b_stop() calls\n",agent->index);

		/*#ifdef DEBUG
      printf(" b_stop is executed for %d\n",index);
#endif  */
	Scheduler &s = Scheduler::instance();

	s.cancel(&intr);
	
}
void
MyTimer2::handle(Event*) {
    //printf("Node[%d] MyTimer::handle(Event*)\n", agent->index);
  // agent->sendMyHello();
    b_stop();
}
void
MyTimer2::b_start(double waiting_time) {

	/*#ifdef DEBUG
      printf(" b_start is executed for %d\n",index);
#endif */ 
//   double interval = agent->hellointerval_;
    //printf("id[%d] b_start() calls\n",agent->index);
   double interval = waiting_time;
   assert(interval >= 0);
   Scheduler::instance().schedule(this, &intr, interval);
}

void
MyTimer2::b_stop(void) {
        //printf("id[%d] b_stop() calls\n",agent->index);

		#ifdef DEBUG
      printf(" b_stop is executed for %d\n",agent->index);
#endif  
	Scheduler &s = Scheduler::instance();

	s.cancel(&intr);
	
}


//bjs end

void
NeighborTimer::handle(Event*) {
  agent->nb_purge();
  Scheduler::instance().schedule(this, &intr, HELLO_INTERVAL);
}

void
RouteCacheTimer::handle(Event*) {
  agent->rt_purge();
#define FREQUENCY 0.5 // sec
  Scheduler::instance().schedule(this, &intr, FREQUENCY);
}

void
	Waittimer1::expire(Event*) {
	#ifdef DEBUG
	printf("N (%.6f): timer1 is cancelled by %d.................................................... \n", CURRENT_TIME, agent->index);
	#endif 
	//agent->timeout(0);
	}


// called to reschedule a new broadcast
	void
	AODV::timeout(int) {
	//pkt_timer_.resched(DEFAULT_BEACON_INTERVAL	);
#ifdef DEBUG
	printf("N (%.6f): timer1 is cancelled by %d.................................................... \n", CURRENT_TIME, index);
	#endif 
	
	//wait_timer_.cancel();
	}

void
	Waittimer2::expire(Event*) {
	//agent->timeout(0);

	#ifdef DEBUG
	printf("N (%.6f): timer2 is cancelled by %d.................................................... \n", CURRENT_TIME, agent->index);
	#endif 

	}





void
LocalRepairTimer::handle(Event* p)  {  // SRD: 5/4/99
aodv_rt_entry *rt;
struct hdr_ip *ih = HDR_IP( (Packet *)p);

   /* you get here after the timeout in a local repair attempt */
   /*	fprintf(stderr, "%s\n", __FUNCTION__); */


    rt = agent->rtable.rt_lookup(ih->daddr());
	
    if (rt && rt->rt_flags != RTF_UP) {
    // route is yet to be repaired
    // I will be conservative and bring down the route
    // and send route errors upstream.
    /* The following assert fails, not sure why */
    /* assert (rt->rt_flags == RTF_IN_REPAIR); */
		
      //rt->rt_seqno++;
      agent->rt_down(rt);
      // send RERR
#ifdef DEBUG
      fprintf(stderr,"Node %d: Dst - %d, failed local repair\n",index, rt->rt_dst);
#endif      
    }
    Packet::free((Packet *)p);
}


/*
   Broadcast ID Management  Functions
*/


void
AODV::id_insert(nsaddr_t id, u_int32_t bid) {
BroadcastID *b = new BroadcastID(id, bid);

 assert(b);
 b->expire = CURRENT_TIME + BCAST_ID_SAVE;
 LIST_INSERT_HEAD(&bihead, b, link);
}

/* SRD */
bool
AODV::id_lookup(nsaddr_t id, u_int32_t bid) {
BroadcastID *b = bihead.lh_first;
 
 // Search the list for a match of source and bid
 for( ; b; b = b->link.le_next) {
   if ((b->src == id) && (b->id == bid))
     return true;     
 }
 return false;
}

//bjs begin
void
AODV::id_insert(u_int32_t bid) {
BroadcastID *b = new BroadcastID(0, bid);

 assert(b);
 b->expire = CURRENT_TIME + BCAST_ID_SAVE;

 LIST_INSERT_HEAD(&bihead, b, link);
}

/* SRD */
bool
AODV::id_lookup(u_int32_t bid) {
BroadcastID *b = bihead.lh_first;
 
 // Search the list for a match of source and bid
 for( ; b; b = b->link.le_next) {
   if (b->id == bid)
     return true;     
 }
 return false;
}

//bjs end

void
AODV::id_purge() {
BroadcastID *b = bihead.lh_first;
BroadcastID *bn;
double now = CURRENT_TIME;

 for(; b; b = bn) {
   bn = b->link.le_next;
   if(b->expire <= now) {
     LIST_REMOVE(b,link);
     delete b;
   }
 }
}

/*
  Helper Functions
*/

double
AODV::PerHopTime(aodv_rt_entry *rt) {
int num_non_zero = 0, i;
double total_latency = 0.0;

 if (!rt)
   return ((double) NODE_TRAVERSAL_TIME );
	
 for (i=0; i < MAX_HISTORY; i++) {
   if (rt->rt_disc_latency[i] > 0.0) {
      num_non_zero++;
      total_latency += rt->rt_disc_latency[i];
   }
 }
 if (num_non_zero > 0)
   return(total_latency / (double) num_non_zero);
 else
   return((double) NODE_TRAVERSAL_TIME);

}

/*
  Link Failure Management Functions
*/

static void
aodv_rt_failed_callback(Packet *p, void *arg) {
  ((AODV*) arg)->rt_ll_failed(p);
}

/*
 * This routine is invoked when the link-layer reports a route failed.
 */
void
AODV::rt_ll_failed(Packet *p) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
aodv_rt_entry *rt;
nsaddr_t broken_nbr = ch->next_hop_;

#ifndef AODV_LINK_LAYER_DETECTION
 drop(p, DROP_RTR_MAC_CALLBACK);
#else 

 /*
  * Non-data packets and Broadcast Packets can be dropped.
  */
  if(! DATA_PACKET(ch->ptype()) ||
     (u_int32_t) ih->daddr() == IP_BROADCAST) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_broke(p);
	if((rt = rtable.rt_lookup(ih->daddr())) == 0) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_del(ch->next_hop_);

#ifdef AODV_LOCAL_REPAIR
  /* if the broken link is closer to the dest than source, 
     attempt a local repair. Otherwise, bring down the route. */


  if (ch->num_forwards() > rt->rt_hops) {
    local_rt_repair(rt, p); // local repair
    // retrieve all the packets in the ifq using this link,
    // queue the packets for which local repair is done, 
    return;
  }
  else	
#endif // LOCAL REPAIR	

  {
    drop(p, DROP_RTR_MAC_CALLBACK);
    // Do the same thing for other packets in the interface queue using the
    // broken link -Mahesh
while((p = ifqueue->filter(broken_nbr))) {
     drop(p, DROP_RTR_MAC_CALLBACK);
    }	
    nb_delete(broken_nbr);
  }

#endif // LINK LAYER DETECTION
}

void
AODV::handle_link_failure(nsaddr_t id) {
aodv_rt_entry *rt, *rtn;
Packet *rerr = Packet::alloc();
struct hdr_aodv_error *re = HDR_AODV_ERROR(rerr);

 re->DestCount = 0;
 for(rt = rtable.head(); rt; rt = rtn) {  // for each rt entry
   rtn = rt->rt_link.le_next; 
   if ((rt->rt_hops != INFINITY2) && (rt->rt_nexthop == id) ) {
     assert (rt->rt_flags == RTF_UP);
     assert((rt->rt_seqno%2) == 0);
     rt->rt_seqno++;
     re->unreachable_dst[re->DestCount] = rt->rt_dst;
     re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
#ifdef DEBUG
     fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\n", __FUNCTION__, CURRENT_TIME,
		     index, re->unreachable_dst[re->DestCount],
		     re->unreachable_dst_seqno[re->DestCount], rt->rt_nexthop);
#endif // DEBUG
     re->DestCount += 1;
     rt_down(rt);
   }
   // remove the lost neighbor from all the precursor lists
   rt->pc_delete(id);
 }   

 if (re->DestCount > 0) {
#ifdef DEBUG
   fprintf(stderr, "%s(%f): %d\tsending RERR...\n", __FUNCTION__, CURRENT_TIME, index);
#endif // DEBUG
   sendError(rerr, false);
 }
 else {
   Packet::free(rerr);
 }
}

void
AODV::local_rt_repair(aodv_rt_entry *rt, Packet *p) {
#ifdef DEBUG
  fprintf(stderr,"%s: Dst - %d\n", __FUNCTION__, rt->rt_dst); 
#endif  
  // Buffer the packet 
  rqueue.enque(p);

  // mark the route as under repair 
  rt->rt_flags = RTF_IN_REPAIR;

  sendRequest(rt->rt_dst);

  // set up a timer interrupt
  Scheduler::instance().schedule(&lrtimer, p->copy(), rt->rt_req_timeout);
}

void
AODV::rt_update(aodv_rt_entry *rt, u_int32_t seqnum, u_int16_t metric,
	       	nsaddr_t nexthop, double expire_time) {

     rt->rt_seqno = seqnum;
     rt->rt_hops = metric;
     rt->rt_flags = RTF_UP;
     rt->rt_nexthop = nexthop;
     rt->rt_expire = expire_time;
}

void
AODV::rt_down(aodv_rt_entry *rt) {
  /*
   *  Make sure that you don't "down" a route more than once.
   */

  if(rt->rt_flags == RTF_DOWN) {
    return;
  }

  // assert (rt->rt_seqno%2); // is the seqno odd?
  rt->rt_last_hop_count = rt->rt_hops;
  rt->rt_hops = INFINITY2;
  rt->rt_flags = RTF_DOWN;
  rt->rt_nexthop = 0;
  rt->rt_expire = 0;

} /* rt_down function */

/*
  Route Handling Functions
*/

void
AODV::rt_resolve(Packet *p) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
aodv_rt_entry *rt;

 /*
  *  Set the transmit failure callback.  That
  *  won't change.
  */
 ch->xmit_failure_ = aodv_rt_failed_callback;
 ch->xmit_failure_data_ = (void*) this;
	rt = rtable.rt_lookup(ih->daddr());
 if(rt == 0) {
	  rt = rtable.rt_add(ih->daddr());
 }

 /*
  * If the route is up, forward the packet 
  */
	
 if(rt->rt_flags == RTF_UP) {
   assert(rt->rt_hops != INFINITY2);
   forward(rt, p, NO_DELAY);
 }
 /*
  *  if I am the source of the packet, then do a Route Request.
  */
	else if(ih->saddr() == index) {
   rqueue.enque(p);
   sendRequest(rt->rt_dst);
 }
 /*
  *	A local repair is in progress. Buffer the packet. 
  */
 else if (rt->rt_flags == RTF_IN_REPAIR) {
   rqueue.enque(p);
 }

 /*
  * I am trying to forward a packet for someone else to which
  * I don't have a route.
  */
 else {
 Packet *rerr = Packet::alloc();
 struct hdr_aodv_error *re = HDR_AODV_ERROR(rerr);
 /* 
  * For now, drop the packet and send error upstream.
  * Now the route errors are broadcast to upstream
  * neighbors - Mahesh 09/11/99
  */	
 
   assert (rt->rt_flags == RTF_DOWN);
   re->DestCount = 0;
   re->unreachable_dst[re->DestCount] = rt->rt_dst;
   re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
   re->DestCount += 1;
#ifdef DEBUG
   fprintf(stderr, "%s: sending RERR...\n", __FUNCTION__);
#endif
   sendError(rerr, false);

   drop(p, DROP_RTR_NO_ROUTE);
 }

}

void
AODV::rt_purge() {
aodv_rt_entry *rt, *rtn;
double now = CURRENT_TIME;
double delay = 0.0;
Packet *p;

 for(rt = rtable.head(); rt; rt = rtn) {  // for each rt entry
   rtn = rt->rt_link.le_next;
   if ((rt->rt_flags == RTF_UP) && (rt->rt_expire < now)) {
   // if a valid route has expired, purge all packets from 
   // send buffer and invalidate the route.                    
	assert(rt->rt_hops != INFINITY2);
     while((p = rqueue.deque(rt->rt_dst))) {
#ifdef DEBUG
       fprintf(stderr, "%s: calling drop()\n",
                       __FUNCTION__);
#endif // DEBUG
       drop(p, DROP_RTR_NO_ROUTE);
     }
     rt->rt_seqno++;
     assert (rt->rt_seqno%2);
     rt_down(rt);
   }
   else if (rt->rt_flags == RTF_UP) {
   // If the route is not expired,
   // and there are packets in the sendbuffer waiting,
   // forward them. This should not be needed, but this extra 
   // check does no harm.
     assert(rt->rt_hops != INFINITY2);
     while((p = rqueue.deque(rt->rt_dst))) {
       forward (rt, p, delay);
       delay += ARP_DELAY;
     }
   } 
   else if (rqueue.find(rt->rt_dst))
   // If the route is down and 
   // if there is a packet for this destination waiting in
   // the sendbuffer, then send out route request. sendRequest
   // will check whether it is time to really send out request
   // or not.
   // This may not be crucial to do it here, as each generated 
   // packet will do a sendRequest anyway.

     sendRequest(rt->rt_dst); 
   }

}

/*
  Packet Reception Routines
*/

void
AODV::recv(Packet *p, Handler*) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);

 assert(initialized());
 //assert(p->incoming == 0);
 // XXXXX NOTE: use of incoming flag has been depracated; In order to track direction of pkt flow, direction_ in hdr_cmn is used instead. see packet.h for details.

 if(ch->ptype() == PT_AODV) {
   ih->ttl_ -= 1;
   recvAODV(p);
   return;
 }


 /*
  *  Must be a packet I'm originating...
  */
if((ih->saddr() == index) && (ch->num_forwards() == 0)) {
 /*
  * Add the IP Header.  
  * TCP adds the IP header too, so to avoid setting it twice, we check if
  * this packet is not a TCP or ACK segment.
  */
  if (ch->ptype() != PT_TCP && ch->ptype() != PT_ACK) {
    ch->size() += IP_HDR_LEN;
  }
   // Added by Parag Dadhania && John Novatnack to handle broadcasting
  if ( (u_int32_t)ih->daddr() != IP_BROADCAST) {
    ih->ttl_ = NETWORK_DIAMETER;
  }
}
 /*
  *  I received a packet that I sent.  Probably
  *  a routing loop.
  */
else if(ih->saddr() == index) {
   drop(p, DROP_RTR_ROUTE_LOOP);
   return;
 }
 /*
  *  Packet I'm forwarding...
  */
 else {
 /*
  *  Check the TTL.  If it is zero, then discard.
  */
   if(--ih->ttl_ == 0) {
     drop(p, DROP_RTR_TTL);
     return;
   }
 }
// Added by Parag Dadhania && John Novatnack to handle broadcasting
 if ( (u_int32_t)ih->daddr() != IP_BROADCAST)
   rt_resolve(p);
 else
   forward((aodv_rt_entry*) 0, p, NO_DELAY);
}


void
AODV::recvAODV(Packet *p) {
 struct hdr_aodv *ah = HDR_AODV(p);

 assert(HDR_IP (p)->sport() == RT_PORT);
 assert(HDR_IP (p)->dport() == RT_PORT);


 /*
  * Incoming Packets.
  */
 switch(ah->ah_type) {

	

 case AODVTYPE_RREQ:
   recvRequest(p);
   break;

 case AODVTYPE_RREP:
   recvReply(p);
   break;

 case AODVTYPE_RERR:
   recvError(p);
   break;

 case AODVTYPE_HELLO:
   recvHello(p);
   break;
 case AODVTYPE_MY_DATA:
    recvMyHello(p);
    break;
        
 default:
   fprintf(stderr, "Invalid AODV type (%x)\n", ah->ah_type);
   exit(1);
 }

}


void
AODV::recvRequest(Packet *p) {
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_request *rq = HDR_AODV_REQUEST(p);
aodv_rt_entry *rt;

  /*
   * Drop if:
   *      - I'm the source
   *      - I recently heard this request.
   */

  if(rq->rq_src == index) {
#ifdef DEBUG
    fprintf(stderr, "%s: got my own REQUEST\n", __FUNCTION__);
#endif // DEBUG
    Packet::free(p);
    return;
  } 

 if (id_lookup(rq->rq_src, rq->rq_bcast_id)) {

#ifdef DEBUG
   fprintf(stderr, "%s: discarding request\n", __FUNCTION__);
#endif // DEBUG
 
   Packet::free(p);
   return;
 }

 /*
  * Cache the broadcast ID
  */
 id_insert(rq->rq_src, rq->rq_bcast_id);



 /* 
  * We are either going to forward the REQUEST or generate a
  * REPLY. Before we do anything, we make sure that the REVERSE
  * route is in the route table.
  */
 aodv_rt_entry *rt0; // rt0 is the reverse route 
   
   rt0 = rtable.rt_lookup(rq->rq_src);
   if(rt0 == 0) { /* if not in the route table */
   // create an entry for the reverse route.
     rt0 = rtable.rt_add(rq->rq_src);
   }
  
   rt0->rt_expire = max(rt0->rt_expire, (CURRENT_TIME + REV_ROUTE_LIFE));

   if ( (rq->rq_src_seqno > rt0->rt_seqno ) ||
    	((rq->rq_src_seqno == rt0->rt_seqno) && 
	 (rq->rq_hop_count < rt0->rt_hops)) ) {
   // If we have a fresher seq no. or lesser #hops for the 
   // same seq no., update the rt entry. Else don't bother.
rt_update(rt0, rq->rq_src_seqno, rq->rq_hop_count, ih->saddr(),
     	       max(rt0->rt_expire, (CURRENT_TIME + REV_ROUTE_LIFE)) );
     if (rt0->rt_req_timeout > 0.0) {
     // Reset the soft state and 
     // Set expiry time to CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT
     // This is because route is used in the forward direction,
     // but only sources get benefited by this change
       rt0->rt_req_cnt = 0;
       rt0->rt_req_timeout = 0.0; 
       rt0->rt_req_last_ttl = rq->rq_hop_count;
       //bjs begin
   rt0->rt_expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
//       rt0->rt_expire = CURRENT_TIME + active_route_time;
       //bjs end
     }

     /* Find out whether any buffered packet can benefit from the 
      * reverse route.
      * May need some change in the following code - Mahesh 09/11/99
      */
     assert (rt0->rt_flags == RTF_UP);
     Packet *buffered_pkt;
     while ((buffered_pkt = rqueue.deque(rt0->rt_dst))) {
       if (rt0 && (rt0->rt_flags == RTF_UP)) {
	assert(rt0->rt_hops != INFINITY2);
         forward(rt0, buffered_pkt, NO_DELAY);
       }
     }
   } 
   // End for putting reverse route in rt table


 /*
  * We have taken care of the reverse route stuff.
  * Now see whether we can send a route reply. 
  */

 rt = rtable.rt_lookup(rq->rq_dst);

 // First check if I am the destination ..

 if(rq->rq_dst == index) {

#ifdef DEBUG
   fprintf(stderr, "%d - %s: destination sending reply\n",
                   index, __FUNCTION__);
#endif // DEBUG

               
   // Just to be safe, I use the max. Somebody may have
   // incremented the dst seqno.
   seqno = max(seqno, rq->rq_dst_seqno)+1;
   if (seqno%2) seqno++;

   sendReply(rq->rq_src,           // IP Destination
             1,                    // Hop Count
             index,                // Dest IP Address
             seqno,                // Dest Sequence Num
             MY_ROUTE_TIMEOUT,     // Lifetime
             rq->rq_timestamp);    // timestamp
 
   Packet::free(p);
 }

 // I am not the destination, but I may have a fresh enough route.

 else if (rt && (rt->rt_hops != INFINITY2) && 
	  	(rt->rt_seqno >= rq->rq_dst_seqno) ) {

   //assert (rt->rt_flags == RTF_UP);
   assert(rq->rq_dst == rt->rt_dst);
   //assert ((rt->rt_seqno%2) == 0);	// is the seqno even?
   sendReply(rq->rq_src,
             rt->rt_hops + 1,
             rq->rq_dst,
             rt->rt_seqno,
	     (u_int32_t) (rt->rt_expire - CURRENT_TIME),
	     //             rt->rt_expire - CURRENT_TIME,
             rq->rq_timestamp);
   // Insert nexthops to RREQ source and RREQ destination in the
   // precursor lists of destination and source respectively
   rt->pc_insert(rt0->rt_nexthop); // nexthop to RREQ source
   rt0->pc_insert(rt->rt_nexthop); // nexthop to RREQ destination

#ifdef RREQ_GRAT_RREP  

   sendReply(rq->rq_dst,
             rq->rq_hop_count,
             rq->rq_src,
             rq->rq_src_seqno,
	     (u_int32_t) (rt->rt_expire - CURRENT_TIME),
	     //             rt->rt_expire - CURRENT_TIME,
             rq->rq_timestamp);
#endif
   
// TODO: send grat RREP to dst if G flag set in RREQ using rq->rq_src_seqno, rq->rq_hop_counT
   
// DONE: Included gratuitous replies to be sent as per IETF aodv draft specification. As of now, G flag has not been dynamically used and is always set or reset in aodv-packet.h --- Anant Utgikar, 09/16/02.

	Packet::free(p);
 }
 /*
  * Can't reply. So forward the  Route Request
  */
 else {
   ih->saddr() = index;
   ih->daddr() = IP_BROADCAST;
   rq->rq_hop_count += 1;
   // Maximum sequence number seen en route
   if (rt) rq->rq_dst_seqno = max(rt->rt_seqno, rq->rq_dst_seqno);
   forward((aodv_rt_entry*) 0, p, DELAY);
 }

}


void
AODV::recvReply(Packet *p) {
//struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
aodv_rt_entry *rt;
char suppress_reply = 0;
double delay = 0.0;
	
#ifdef DEBUG
 fprintf(stderr, "%d - %s: received a REPLY\n", index, __FUNCTION__);
#endif // DEBUG


 /*
  *  Got a reply. So reset the "soft state" maintained for 
  *  route requests in the request table. We don't really have
  *  have a separate request table. It is just a part of the
  *  routing table itself. 
  */
 // Note that rp_dst is the dest of the data packets, not the
 // the dest of the reply, which is the src of the data packets.

 rt = rtable.rt_lookup(rp->rp_dst);
        
 /*
  *  If I don't have a rt entry to this host... adding
  */
 if(rt == 0) {
   rt = rtable.rt_add(rp->rp_dst);
 }

 /*
  * Add a forward route table entry... here I am following 
  * Perkins-Royer AODV paper almost literally - SRD 5/99
  */

 if ( (rt->rt_seqno < rp->rp_dst_seqno) ||   // newer route 
      ((rt->rt_seqno == rp->rp_dst_seqno) &&  
       (rt->rt_hops > rp->rp_hop_count)) ) { // shorter or better route
	
  // Update the rt entry 
  rt_update(rt, rp->rp_dst_seqno, rp->rp_hop_count,
		rp->rp_src, CURRENT_TIME + rp->rp_lifetime);

  // reset the soft state
  rt->rt_req_cnt = 0;
  rt->rt_req_timeout = 0.0; 
  rt->rt_req_last_ttl = rp->rp_hop_count;
  
if (ih->daddr() == index) { // If I am the original source
  // Update the route discovery latency statistics
  // rp->rp_timestamp is the time of request origination
		
    rt->rt_disc_latency[(unsigned char)rt->hist_indx] = (CURRENT_TIME - rp->rp_timestamp)
                                         / (double) rp->rp_hop_count;
    // increment indx for next time
    rt->hist_indx = (rt->hist_indx + 1) % MAX_HISTORY;
  }	

  /*
   * Send all packets queued in the sendbuffer destined for
   * this destination. 
   * XXX - observe the "second" use of p.
   */
  Packet *buf_pkt;
  while((buf_pkt = rqueue.deque(rt->rt_dst))) {
    if(rt->rt_hops != INFINITY2) {
          assert (rt->rt_flags == RTF_UP);
    // Delay them a little to help ARP. Otherwise ARP 
    // may drop packets. -SRD 5/23/99
      forward(rt, buf_pkt, delay);
      delay += ARP_DELAY;
    }
  }
 }
 else {
  suppress_reply = 1;
 }

 /*
  * If reply is for me, discard it.
  */

if(ih->daddr() == index || suppress_reply) {
   Packet::free(p);
 }
 /*
  * Otherwise, forward the Route Reply.
  */
 else {
 // Find the rt entry
aodv_rt_entry *rt0 = rtable.rt_lookup(ih->daddr());
   // If the rt is up, forward
   if(rt0 && (rt0->rt_hops != INFINITY2)) {
        assert (rt0->rt_flags == RTF_UP);
     rp->rp_hop_count += 1;
     rp->rp_src = index;
     forward(rt0, p, NO_DELAY);
     // Insert the nexthop towards the RREQ source to 
     // the precursor list of the RREQ destination
     rt->pc_insert(rt0->rt_nexthop); // nexthop to RREQ source
     
   }
   else {
   // I don't know how to forward .. drop the reply. 
#ifdef DEBUG
     fprintf(stderr, "%s: dropping Route Reply\n", __FUNCTION__);
#endif // DEBUG
     drop(p, DROP_RTR_NO_ROUTE);
   }
 }
}


void
AODV::recvError(Packet *p) {
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_error *re = HDR_AODV_ERROR(p);
aodv_rt_entry *rt;
u_int8_t i;
Packet *rerr = Packet::alloc();
struct hdr_aodv_error *nre = HDR_AODV_ERROR(rerr);

 nre->DestCount = 0;

 for (i=0; i<re->DestCount; i++) {
 // For each unreachable destination
   rt = rtable.rt_lookup(re->unreachable_dst[i]);
   if ( rt && (rt->rt_hops != INFINITY2) &&
	(rt->rt_nexthop == ih->saddr()) &&
     	(rt->rt_seqno <= re->unreachable_dst_seqno[i]) ) {
	assert(rt->rt_flags == RTF_UP);
	assert((rt->rt_seqno%2) == 0); // is the seqno even?
#ifdef DEBUG
     fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\t(%d\t%u\t%d)\n", __FUNCTION__,CURRENT_TIME,
		     index, rt->rt_dst, rt->rt_seqno, rt->rt_nexthop,
		     re->unreachable_dst[i],re->unreachable_dst_seqno[i],
	             ih->saddr());
#endif // DEBUG
     	rt->rt_seqno = re->unreachable_dst_seqno[i];
     	rt_down(rt);

   // Not sure whether this is the right thing to do
   Packet *pkt;
	while((pkt = ifqueue->filter(ih->saddr()))) {
        	drop(pkt, DROP_RTR_MAC_CALLBACK);
     	}

     // if precursor list non-empty add to RERR and delete the precursor list
     	if (!rt->pc_empty()) {
     		nre->unreachable_dst[nre->DestCount] = rt->rt_dst;
     		nre->unreachable_dst_seqno[nre->DestCount] = rt->rt_seqno;
     		nre->DestCount += 1;
		rt->pc_delete();
     	}
   }
 } 

 if (nre->DestCount > 0) {
#ifdef DEBUG
   fprintf(stderr, "%s(%f): %d\t sending RERR...\n", __FUNCTION__, CURRENT_TIME, index);
#endif // DEBUG
   sendError(rerr);
 }
 else {
   Packet::free(rerr);
 }

 Packet::free(p);
}


/*
   Packet Transmission Routines
*/

void
AODV::forward(aodv_rt_entry *rt, Packet *p, double delay) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);

 if(ih->ttl_ == 0) {

#ifdef DEBUG
  fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
#endif // DEBUG
 
  drop(p, DROP_RTR_TTL);
  return;
 }

 if (ch->ptype() != PT_AODV && ch->direction() == hdr_cmn::UP &&
	((u_int32_t)ih->daddr() == IP_BROADCAST)
		|| (ih->daddr() == here_.addr_)) {
	dmux_->recv(p,0);
	return;
 }

 if (rt) {
   assert(rt->rt_flags == RTF_UP);
   //bjs begin
rt->rt_expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
  // rt->rt_expire = CURRENT_TIME + active_route_time;
   //bjs end
   ch->next_hop_ = rt->rt_nexthop;
   ch->addr_type() = NS_AF_INET;
   ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
 }
 else { // if it is a broadcast packet
   // assert(ch->ptype() == PT_AODV); // maybe a diff pkt type like gaf
   assert(ih->daddr() == (nsaddr_t) IP_BROADCAST);
   ch->addr_type() = NS_AF_NONE;
   ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
 }

if (ih->daddr() == (nsaddr_t) IP_BROADCAST) {
 // If it is a broadcast packet
   assert(rt == 0);
   if (ch->ptype() == PT_AODV) {
     /*
      *  Jitter the sending of AODV broadcast packets by 10ms
      */
     Scheduler::instance().schedule(target_, p,
      				   0.01 * Random::uniform());
   } else {
     Scheduler::instance().schedule(target_, p, 0.);  // No jitter
   }
 }
 else { // Not a broadcast packet 
   if(delay > 0.0) {
     Scheduler::instance().schedule(target_, p, delay);
   }
   else {
   // Not a broadcast packet, no delay, send immediately
     Scheduler::instance().schedule(target_, p, 0.);
   }
 }

}


void
AODV::sendRequest(nsaddr_t dst) {
// Allocate a RREQ packet 
Packet *p = Packet::alloc();
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_request *rq = HDR_AODV_REQUEST(p);
aodv_rt_entry *rt = rtable.rt_lookup(dst);

 assert(rt);

 /*
  *  Rate limit sending of Route Requests. We are very conservative
  *  about sending out route requests. 
  */

 if (rt->rt_flags == RTF_UP) {
   assert(rt->rt_hops != INFINITY2);
   Packet::free((Packet *)p);
   return;
 }

 if (rt->rt_req_timeout > CURRENT_TIME) {
   Packet::free((Packet *)p);
   return;
 }

 // rt_req_cnt is the no. of times we did network-wide broadcast
 // RREQ_RETRIES is the maximum number we will allow before 
 // going to a long timeout.

 if (rt->rt_req_cnt > RREQ_RETRIES) {
   rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
   rt->rt_req_cnt = 0;
 Packet *buf_pkt;
   while ((buf_pkt = rqueue.deque(rt->rt_dst))) {
       drop(buf_pkt, DROP_RTR_NO_ROUTE);
   }
   Packet::free((Packet *)p);
   return;
 }

#ifdef DEBUG
   fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d\n",
                    ++route_request, index, rt->rt_dst);
#endif // DEBUG

 // Determine the TTL to be used this time. 
 // Dynamic TTL evaluation - SRD

 rt->rt_req_last_ttl = max(rt->rt_req_last_ttl,rt->rt_last_hop_count);

 if (0 == rt->rt_req_last_ttl) {
 // first time query broadcast
   ih->ttl_ = TTL_START;
 }
 else {
 // Expanding ring search.
   if (rt->rt_req_last_ttl < TTL_THRESHOLD)
     ih->ttl_ = rt->rt_req_last_ttl + TTL_INCREMENT;
   else {
   // network-wide broadcast
     ih->ttl_ = NETWORK_DIAMETER;
     rt->rt_req_cnt += 1;
   }
 }

 // remember the TTL used  for the next time
 rt->rt_req_last_ttl = ih->ttl_;

 // PerHopTime is the roundtrip time per hop for route requests.
 // The factor 2.0 is just to be safe .. SRD 5/22/99
 // Also note that we are making timeouts to be larger if we have 
 // done network wide broadcast before. 

 rt->rt_req_timeout = 2.0 * (double) ih->ttl_ * PerHopTime(rt); 
 if (rt->rt_req_cnt > 0)
   rt->rt_req_timeout *= rt->rt_req_cnt;
 rt->rt_req_timeout += CURRENT_TIME;

 // Don't let the timeout to be too large, however .. SRD 6/8/99
 if (rt->rt_req_timeout > CURRENT_TIME + MAX_RREQ_TIMEOUT)
   rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
 rt->rt_expire = 0;

#ifdef DEBUG
 fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d, tout %f ms\n",
	         ++route_request, 
		 index, rt->rt_dst, 
		 rt->rt_req_timeout - CURRENT_TIME);
#endif	// DEBUG
	

 // Fill out the RREQ packet 
 // ch->uid() = 0;
 ch->ptype() = PT_AODV;
 ch->size() = IP_HDR_LEN + rq->size();
 ch->iface() = -2;
 ch->error() = 0;
 ch->addr_type() = NS_AF_NONE;
 ch->prev_hop_ = index;          // AODV hack

 ih->saddr() = index;
 ih->daddr() = IP_BROADCAST;
 ih->sport() = RT_PORT;
 ih->dport() = RT_PORT;

 // Fill up some more fields. 
 rq->rq_type = AODVTYPE_RREQ;
 rq->rq_hop_count = 1;
 rq->rq_bcast_id = bid++;
 rq->rq_dst = dst;
 rq->rq_dst_seqno = (rt ? rt->rt_seqno : 0);
 rq->rq_src = index;
 seqno += 2;
 assert ((seqno%2) == 0);
 rq->rq_src_seqno = seqno;
 rq->rq_timestamp = CURRENT_TIME;

 Scheduler::instance().schedule(target_, p, 0.);

}

void
AODV::sendReply(nsaddr_t ipdst, u_int32_t hop_count, nsaddr_t rpdst,
                u_int32_t rpseq, u_int32_t lifetime, double timestamp) {
Packet *p = Packet::alloc();
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
aodv_rt_entry *rt = rtable.rt_lookup(ipdst);

#ifdef DEBUG
fprintf(stderr, "sending Reply from %d at %.2f\n", index, Scheduler::instance().clock());
#endif // DEBUG
 assert(rt);

 rp->rp_type = AODVTYPE_RREP;
 //rp->rp_flags = 0x00;
 rp->rp_hop_count = hop_count;
 rp->rp_dst = rpdst;
 rp->rp_dst_seqno = rpseq;
 rp->rp_src = index;
 rp->rp_lifetime = lifetime;
 rp->rp_timestamp = timestamp;
   
 // ch->uid() = 0;
 ch->ptype() = PT_AODV;
 ch->size() = IP_HDR_LEN + rp->size();
 ch->iface() = -2;
 ch->error() = 0;
 ch->addr_type() = NS_AF_INET;
 ch->next_hop_ = rt->rt_nexthop;
 ch->prev_hop_ = index;          // AODV hack
 ch->direction() = hdr_cmn::DOWN;

 ih->saddr() = index;
 ih->daddr() = ipdst;
 ih->sport() = RT_PORT;
 ih->dport() = RT_PORT;
 ih->ttl_ = NETWORK_DIAMETER;

 Scheduler::instance().schedule(target_, p, 0.);

}

void
AODV::sendError(Packet *p, bool jitter) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_error *re = HDR_AODV_ERROR(p);
    
#ifdef ERROR
fprintf(stderr, "sending Error from %d at %.2f\n", index, Scheduler::instance().clock());
#endif // DEBUG

 re->re_type = AODVTYPE_RERR;
 //re->reserved[0] = 0x00; re->reserved[1] = 0x00;
 // DestCount and list of unreachable destinations are already filled

 // ch->uid() = 0;
 ch->ptype() = PT_AODV;
 ch->size() = IP_HDR_LEN + re->size();
 ch->iface() = -2;
 ch->error() = 0;
 ch->addr_type() = NS_AF_NONE;
 ch->next_hop_ = 0;
 ch->prev_hop_ = index;          // AODV hack
 ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction

 ih->saddr() = index;
 ih->daddr() = IP_BROADCAST;
 ih->sport() = RT_PORT;
 ih->dport() = RT_PORT;
 ih->ttl_ = 1;

 // Do we need any jitter? Yes
 if (jitter)
 	Scheduler::instance().schedule(target_, p, 0.01*Random::uniform());
 else
 	Scheduler::instance().schedule(target_, p, 0.0);

}


/*
   Neighbor Management Functions
*/

inline double distance(double x1, double y1, double x2, double y2)
				
	{
	return sqrt((x1 - x2)*(x1 - x2) +
			(y1 - y2)*(y1 - y2) );
	}
void
AODV::sendHello() {
if(helloOnOff_)
    {
    Packet *p = Packet::alloc();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    //bjs begin
    //struct hdr_aodv_reply *rh = HDR_AODV_REPLY(p);
    struct hdr_aodv_hello *rh = HDR_AODV_HELLO(p);
    Node *thisnode = Node::get_node_by_address(index); //inserted by Dongwon Kum
    //bjs end
#ifdef DEBUG    
//printf("N (%.6f):sending hellow from %d \n", CURRENT_TIME,index);
#endif // DEBUG

     rh->rp_type = AODVTYPE_HELLO;
     //rh->rp_flags = 0x00;
     rh->rp_hop_count = 1;
     rh->rp_dst = index;
     //bjs begin
    //original rh->rp_dst_seqno = seqno;
     
     rh->rp_lifetime = (1 + ALLOWED_HELLO_LOSS) * HELLO_INTERVAL;
     //fprintf(stderr, "sending Hello from %d at %.2f\n", index, Scheduler::instance().clock());
     //bjs end
    //bjs begin
    double x, y, z;
	 double dx,dy,dz;
    //double chellointerval;
    ((MobileNode *)thisnode)->getLoc(&x, &y, &z);
    rh->x_ = x;
    rh->y_ = y;
    px = x;
    py = y;


	// ajmal 
	// dx contains the direction information (-ve value if opposite direction)
	((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);	
	rh->direction = dx;
    //temp
    //printf("[%f] [%d] sendHello:: X [%f], Y [%f], S [%f], HI [%f]\n", CURRENT_TIME, index, rh->x_, rh->y_, rh->speed_, hellointerval_);
     
    //bjs end

     // ch->uid() = 0;
     ch->ptype() = PT_AODV;
     ch->size() = IP_HDR_LEN + rh->size();
     ch->iface() = -2;
     ch->error() = 0;
     ch->addr_type() = NS_AF_NONE;
     ch->prev_hop_ = index;          // AODV hack

     ih->saddr() = index;
     ih->daddr() = IP_BROADCAST;
     ih->sport() = RT_PORT;
     ih->dport() = RT_PORT;
     ih->ttl_ = 1;

     Scheduler::instance().schedule(target_, p, 0.0);
    }
}


void
AODV::recvHello(Packet *p) {
struct hdr_ip *ih = HDR_IP(p);
//struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);


if(helloOnOff_){
struct hdr_aodv_hello *rp = HDR_AODV_HELLO(p);
    AODV_Neighbor *nb;
	
	double x, y, z;
	 double dx,dy,dz;
	 bool newneighbor=FALSE;

	 Node *thisnode = Node::get_node_by_address(index);
    //double chellointerval;
    ((MobileNode *)thisnode)->getLoc(&x, &y, &z);
#ifdef DEBUG	
//printf("N (%.6f):AODV::recvHello(Packet *p) is executed by %d=%f from %d=%f\n", CURRENT_TIME,index,x, ih->saddr(),rp->x_);
#endif
	 
	((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);
    //bjs begin
    //bjs end
    
     nb = nb_lookup(rp->rp_dst);
     if(nb == 0) {
        //bjs begin
//       nb_insert(rp->rp_dst);
        nb_insert(rp->rp_dst, rp->x_,rp->direction);
		newneighbor=TRUE;
	 
        
     }
     else {
       //nb->nb_expire = CURRENT_TIME +
          //             (1.5 * ALLOWED_HELLO_LOSS * HELLO_INTERVAL);

nb->nb_expire= CURRENT_TIME +1;
	   nb->x=rp->x_;	   nb->direction=rp->direction;
     }

	  if (wait_timer1_.status()!=0 && (nb_lookup(x,dx)==TRUE ||ODC(dx)==TRUE))
	 	{
			#ifdef DEBUG	
			printf(" %d Timer1 = %d MDC = %d  ODC = %d\n", index, wait_timer1_.status(),nb_lookup(x,dx),ODC(dx));
			#endif
			sendMyHello();
			//senddata();
			wait_timer1_.cancel();
			#ifdef DEBUG	
			printf(" Timer1 is cancelled by %d......................................................\n", index);
			#endif
	 	}

	 /*  if (wait_timer2_.status()!=0 && ODC(dx)==TRUE  && newneighbor==TRUE)
	 	{
			#ifdef DEBUG	
			printf("%d Timer2 = %d  ODC = %d\n", index, wait_timer2_.status(),ODC(dx));
			#endif
			sendMyHello();
			//senddata();
			
	 	}*/

		if (wait_timer2_.status()!=0 && rp->direction < 0 && newneighbor==TRUE && rp->x_ > x)
	 	{
			#ifdef DEBUG	
			printf("%d Timer2 = %d  ODC = %d\n", index, wait_timer2_.status(),ODC(dx));
			#endif
			sendMyHello();
			//senddata();
			
	 	}
	
     Packet::free(p);
    }
}




void
AODV::sendMyHello() {
//bjs begin
if(helloFlag_){
    Packet *p = Packet::alloc();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    //bjs begin
    //struct hdr_aodv_reply *rh = HDR_AODV_REPLY(p);
    struct hdr_aodv_hello *rh = HDR_AODV_HELLO(p);
    Node *thisnode = Node::get_node_by_address(index); //inserted by Dongwon Kum
    //bjs end

     


     rh->rp_type = AODVTYPE_MY_DATA;
     //rh->rp_flags = 0x00;
     rh->rp_hop_count = 1;
     rh->rp_dst = index;
	 //printf("I'm node[%d] bjs MYsendHello() false%lf    broadcast id = %d  \n", index, rh->rp_lifetime, bid);
	if (index == source)
	 rh->rq_bcast_id = bid++;
	else rh->rq_bcast_id = message_id;
	 id_insert(rh->rq_bcast_id);
     
     //bjs begin
    //original rh->rp_dst_seqno = seqno;
     
     //rh->rp_lifetime = (1 + ALLOWED_HELLO_LOSS) * HELLO_INTERVAL;
      if (source == index)
    	{
		rh->rp_timestamp =CURRENT_TIME;
		rh->rp_lifetime = 30;
		
    	}
	else
		{
		double spent_time;
		double remaining_lifetime;
   		spent_time= CURRENT_TIME  - received_timestamp;
   		remaining_lifetime= received_lifetime- spent_time;
		/*#ifdef DEBUG  
		printf( "received_timestamp = %f    received_lifetime = %f\n ",received_timestamp,received_lifetime);
printf( "spent time = %f    remaining_lifetime = %f\n ",spent_time,remaining_lifetime);
#endif */
		rh->rp_timestamp =CURRENT_TIME;
		rh->rp_lifetime= remaining_lifetime;
		
		}
     //fprintf(stderr, "sending Hello from %d at %.2f\n", index, Scheduler::instance().clock());
     //printf("bjs %lf", rh->rp_lifetime);
     //bjs end
    //bjs begin

	 	 if (rh->rp_lifetime <1)
{

mtimer.b_stop();
			mtimer2.b_stop();
       Packet::free(p);
       return;
}
    double x, y, z;
	 double dx,dy,dz;
    //double chellointerval;
    ((MobileNode *)thisnode)->getLoc(&x, &y, &z);
    rh->x_ = x;
    rh->y_ = y;
    px = x;
    py = y;
	
    //get the number of node behind
   // rh->cnt_node_behind_sender = nb_lookup(rh->x_);

	// added by ajmal	
	((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);
	rh->direction= dx;
#ifdef DEBUG  
printf( "sending myHello from %d at %.6f  direction = %f position = %f************************************\n", index, Scheduler::instance().clock(),dx,x);
#endif // DEBUG
	  
	rh->MDC= nb_lookup( x,  dx);
    //temp
    //printf("[%f] [I'm %d] MYsendHello[bid: %d]:: X [%f], Y [%f], S [%f], HI [%f], total number of nodes [%d]\n", CURRENT_TIME, index, rh->rq_bcast_id, rh->x_, rh->y_, rh->speed_, hellointerval_, rh->cnt_node_behind_sender);
     
    //bjs end

     // ch->uid() = 0;
     ch->ptype() = PT_AODV;
     ch->size() = IP_HDR_LEN + rh->size();
     ch->iface() = -2;
     ch->error() = 0;
     ch->addr_type() = NS_AF_NONE;
     ch->prev_hop_ = index;          // AODV hack

     ih->saddr() = index;
     ih->daddr() = IP_BROADCAST;
     ih->sport() = RT_PORT;
     ih->dport() = RT_PORT;
     if (index==source)	 
     ih->ttl_ = 250;
	 else
	 	ih->ttl_= timetolive;

	 N = timetolive;

     Scheduler::instance().schedule(target_, p, 0.0);

	

	/* if( dx < 0) 
					return;
				
				wait_timer_.resched(2); // wait for pkt arrival from ODN*/
    }
}


void
	AODV::senddata() {

	#ifdef DEBUG
	printf("forwarding data  from %d at %.6f************************************\n", index, Scheduler::instance().clock());
	#endif 
	Packet *p = rqueue.deque();
	
	struct hdr_cmn *ch = HDR_CMN(p);
	
		
    //struct hdr_ip *ih = HDR_IP(p);
   struct hdr_aodv_hello *hr = HDR_AODV_HELLO(p);
    Node *thisnode = Node::get_node_by_address(index); //inserted by Dongwon Kum
    
    printf ("trying to remove1..............................\n");
	
  //   rh->rp_dst = index;
	     
    double x, y, z;
	 double dx,dy,dz;
    //double chellointerval;
    ((MobileNode *)thisnode)->getLoc(&x, &y, &z);
    hr->x_ = x;
    hr->y_ = y;
    
	
    
	// added by ajmal	
	((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);
	hr->direction= dx;

	hr->MDC= nb_lookup( x,  dx);
    
    
	ch->prev_hop_ = index; 
	
	ch->error() = 0;
	
	ch->direction() = hdr_cmn::DOWN;





	
	
	Scheduler::instance().schedule(target_, p, 0);
	//rqueue.enque(p);
	
}
// By ajmal

void
AODV::recvMyHello(Packet *p) {

struct hdr_aodv_hello *rp = HDR_AODV_HELLO(p);
		  struct hdr_cmn *ch = HDR_CMN(p);
		   struct hdr_ip *ih = HDR_IP(p);

		timetolive = ih->ttl_;
		message_id= rp->rq_bcast_id;
		int Hop = ih->ttl_;
		
		  #ifdef DEBUG
		//printf("N (%.6f):AODV::recvMyHello(Packet *p) is executed by %d from %d\n", CURRENT_TIME,index,  ch->prev_hop_);
		#endif


int check = CURRENT_TIME;
AODV_Neighbor *nb;
    Node *thisnode = Node::get_node_by_address(index); //inserted by Dongwon Kum
    double x, y, z;
    double dx,dy,dz;
	int Ns = 5;
	int Slot;
	
    //double chellointerval;
    ((MobileNode *)thisnode)->getLoc(&x, &y, &z);
     //if (rp->MDC == 0)  // this packet is received from last vehilce.
     //Last_vehicle_position = rp->x_;//

	 
 ////////////////////added for opposite direction//////////////////////

((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);
    
	
	if (  ( dx <0 && rp->direction >0) || (dx>0 && rp->direction<0)   ){     // This vehcile is moving in opposite direction

	#ifdef DEBUG
		//printf(" Timer stop is executed by %d because opposite direction.\n", index);
		#endif
	mtimer.b_stop();
		mtimer.b_stop();
       Packet::free(p);
       return;
    }	



	 
 //duplicate data
  #ifdef DEBUG
		printf(" message id = %d  \n",  rp->rq_bcast_id);
		#endif
    if (id_lookup(rp->rq_bcast_id) && rp->x_ > x) {


			
		 #ifdef DEBUG
		//printf("Timer stopped because duplicate data  from %d Hop = %d  N = %d\n",   ch->prev_hop_,Hop, N);
		#endif
		
		 if (wait_timer2_.status()!=0 && (Hop == N -2) && rp->MDC !=0)//rp->x_> Last_vehicle_position)
	 	{

			#ifdef DEBUG
		//printf("Timer2 is cancelled by %d..............................\n",   index);
		#endif
			wait_timer2_.cancel();
			
	 	}

       mtimer.b_stop();
	/* if (wait_timer_.status()!=0)
	 wait_timer_.cancel();   */
       Packet::free(p);
       return;
     }

		((MobileNode *)thisnode)->getVelo(&dx, &dy, &dz);
    
	 if (id_lookup(rp->rq_bcast_id) && x > rp->x_ ) {
	 	#ifdef DEBUG		
		//printf(" Ignore this packet becaue already received from front\n" );	
		#endif
		Packet::free(p);  
		return;     
		}
	
       
    //it's new message
    id_insert(rp->rq_bcast_id);
    //printf("I'm node[%d] brocast id[%d]  total node cnt from S = [%d] \n", index, rp->rq_bcast_id, rp->cnt_node_behind_sender);

received_timestamp= rp->rp_timestamp;         // must not be written at the beginning 
										//because it will keep the record 
								//when the first time this packet id is received to keep track of life time of this packet.
								//otherwise if mentioned at the beginning, this variable
								// is updated whenever a duplicated message is arrived.
											
		received_lifetime = rp->rp_lifetime;

	if (nb_lookup(x,dx))  // if (MDC =1)
	{

	
       double s = distance(x, y,rp->x_,rp->y_);
      Slot = Ns * ( 1 - min(s, 500)/500.0);

	
	mtimer.b_stop();
    double waiting_deday = Slot * WAITING_TIME;

	#ifdef DEBUG
		//printf("Broadcast suppression applied by  %d and Slot is %d and time is %f\n",   index,Slot,waiting_deday);
		#endif
		//rqueue.enque(p);
    mtimer.b_start(waiting_deday);
    Packet::free(p);
        
		}
	else if (ODC(dx))  // MDC = 0 and ODC = 1 then rebroadcast
		{
			//first check for A whether B is behind or ahead, if behind no need as it would have received the same message.
			if (dx <=0 && ONF( x))
				{
			sendMyHello();
			//rqueue.enque(p);
			//senddata();
				}
			if (dx>0)
				{
					//N = ih->ttl_;
					sendMyHello();
					//rqueue.enque(p);
					//senddata();
				#ifdef DEBUG
				printf("Wait 2 is started ....................................................... \n");
				#endif
				if (wait_timer2_.status()!=0)
			wait_timer2_.cancel();
				wait_timer2_.sched(10);
				
				}

			 
    			Packet::free(p);
				
        
		}
	else {
			
			//sendMyHello();// Let last vehicle to broadcast. otherwise it will cause 2nd last vehicle to broadcast and it will cause the last vehicle to stop its broadcasting process for ever.

			#ifdef DEBUG
		printf("Wait 1 is started ...........................................................\n");
		#endif
		if (wait_timer1_.status()!=0)
			wait_timer1_.cancel();
		wait_timer1_.sched(10);
			//mtimer2.b_start(2); // wait I for 2 seconds, in these 2 seconds it will wait for hello pkt , otherwise it will be expired.
	 	//rqueue.enque(p);
			
    			Packet::free(p);		
			}

		}

	

	

// bjs begin
// insert nb information including x position
void
AODV::nb_insert(nsaddr_t id, double x_val, double direction){
AODV_Neighbor *nb = new AODV_Neighbor(id);
 nb->x = x_val;
 nb->direction = direction;
 assert(nb);
 nb->nb_expire = CURRENT_TIME + 1;
 //printf("this node's information %0.2lf exprire\n",nb->nb_expire);
// printf("nb insert id[%d], x_position[%0.2lf], expiration[%0.2lf]\n", id, x_val, nb->nb_expire);
 LIST_INSERT_HEAD(&nbhead, nb, nb_link);
 seqno += 2;             // set of neighbors changed
 assert ((seqno%2) == 0);
}
// bjs end

AODV_Neighbor*
AODV::nb_lookup(nsaddr_t id) {
AODV_Neighbor *nb = nbhead.lh_first;

 for(; nb; nb = nb->nb_link.le_next) {
   if(nb->nb_addr == id) break;
 }
 return nb;
}



//bjs begin
//aodv.h  int nb_lookup(double x_pos)
int 
AODV::nb_lookup(double my_x,double my_direction) {
AODV_Neighbor *nb = nbhead.lh_first;
 int cnt=0;
 for(; nb; nb = nb->nb_link.le_next) {


    if((nb->x > my_x && nb->direction <= 0 && my_direction<=0) || (nb->x > my_x && nb->direction > 0 && my_direction>0))
    	{
    /*	#ifdef DEBUG		
		printf(" n_id = %d , My_pos = %f , n_pos = %f, n_dir = %f , distance = %f\n ", nb->nb_addr,my_x, nb->x,nb->direction,nb->x -my_x);	
		#endif*/
        return 1;

    	}
 }
 return 0;
// return nb;
}

int 
AODV::ODC(double dx) {
AODV_Neighbor *nb = nbhead.lh_first;
 //int cnt=0;
 for(; nb; nb = nb->nb_link.le_next) {

if (  ( dx <=0 && nb->direction >0) || (dx>0 && nb->direction<=0)   )
       	{
    	#ifdef DEBUG		
		printf(" %f %d n_id = %d ,  n_dir = %f,  n_x = %f \n ", CURRENT_TIME,index, nb->nb_addr,nb->direction,nb->x);	
		#endif
        return 1;

    	}
 }
 return 0;
// return nb;
}
int 
AODV::ONF(double x) {
AODV_Neighbor *nb = nbhead.lh_first;
 //int cnt=0;
 for(; nb; nb = nb->nb_link.le_next) {

if (   x < nb->x  && nb ->direction > 0)
       	{
    	#ifdef DEBUG		
		printf(" n_id = %d ,  n_x = %f, my_x = %f \n ", nb->nb_addr,nb->x,x);	
		#endif
        return 1;

    	}
 }
 return 0;
// return nb;
}

// Ajmal added
// ======================================================================
	//  Calculating Distance between two points
	// ======================================================================
	
	

    bool same_direction (double nb, double source) {
#ifdef DEBUG
		printf("%f  %f \n", nb, source);
		#endif

		if (nb >0 && source > 0)
			return true;
		else if (nb <0 && source < 0)
			
			return true;
			
		else
			return false;
    	}
		
// For ajmal idea

int 
AODV::nb_lookup(double source_x, double source_y, double source_direction, double my_x, double my_y) {
#ifdef DEBUG
		//printf("N (%.6f):AODV::nb_lookup(double source_x, double source_y, double my_x, double my_y) is executed by %d\n", CURRENT_TIME,index);
		#endif

AODV_Neighbor *nb = nbhead.lh_first;

 int cnt=0;
double s;
int fail = 0;




 for(; nb; nb = nb->nb_link.le_next) {


  // MobileNode *pnode = (MobileNode*)Node::get_node_by_address(nb->nb_addr);
	//pnode->update_position();       // update the position, before using it
	//nb->x = pnode->X();     // include current own location
	//nb->y = pnode->Y();


	#ifdef DEBUG
		//printf(" n_id = %d , My_pos = %f , n_pos = %f, n_dir = %f \n ", nb->nb_addr,my_x, nb->x,nb->direction);
		#endif

     s= distance(source_x, source_y,nb->x,nb->y);
     bool check = same_direction (nb->direction, source_direction);

	 
			#ifdef DEBUG
		printf(" check = %d\n", check );
		#endif
	if(s<= 500 && nb->x > my_x && check ){
		
        cnt++;
        }
	else
		 printf (" neighbor id = %d  distance from source = %f  and fail = %d\n", nb->nb_addr,s, ++fail);
  
 }
 printf (" vehicle id = %d and count = %d\n", index, cnt);
 return cnt;

}//return n


void 
AODV::nb_show() {
AODV_Neighbor *nb = nbhead.lh_first;
 for(; nb; nb = nb->nb_link.le_next) {
    //printf("my id[%d] node id = %d, position = %0.2lf expiration = %0.2lf\n", nb->nb_addr, nb->x, nb->nb_expire);
    printf("nb id[%d], x_position[%0.2lf], expiration[%0.2lf]\n", nb->nb_addr, nb->x, nb->nb_expire);
  // if(nb->nb_addr == id) break;
 }
// return nb;
}

//bjs end


/*
 * Called when we receive *explicit* notification that a Neighbor
 * is no longer reachable.
 */
void
AODV::nb_delete(nsaddr_t id) {
AODV_Neighbor *nb = nbhead.lh_first;

 log_link_del(id);
 seqno += 2;     // Set of neighbors changed
 assert ((seqno%2) == 0);

 for(; nb; nb = nb->nb_link.le_next) {
   if(nb->nb_addr == id) {

	#ifdef DEBUG		
		//printf(" neighbor  %d  is deleted by %d\n ", nb->nb_addr,index);	
		#endif
     LIST_REMOVE(nb,nb_link);
     delete nb;
     break;
   }
 }

 handle_link_failure(id);

}


/*
 * Purges all timed-out Neighbor Entries - runs every
 * HELLO_INTERVAL * 1.5 seconds.
 */
void
AODV::nb_purge() {
AODV_Neighbor *nb = nbhead.lh_first;
AODV_Neighbor *nbn;
double now = CURRENT_TIME;

 for(; nb; nb = nbn) {
   nbn = nb->nb_link.le_next;
   if(nb->nb_expire <= now) {
     nb_delete(nb->nb_addr);
   }
 }

}
