BEGIN{
src1="_202_"; dst1="_138_";

  received_packets1 = 0;
total_drops = 0;
  total_delay1 = 0;
  

hello=0;
  total_hops = 0;
  flag1 = 0;
  

drop=0;
  retransmissions = 0;
total_retransmit=0;
  sent_packets1=0;
  

};

{
action=$1;
time = $2;
level = $23;
packet_id = $20;
source = $3;
destination = $3;
Agent = $4;
ttl = $43;

if (action =="s"  && level =="(AODVTYPE_MY_DATA)"  && Agent == "RTR" && source !=src1  )
{
retransmissions++;

};


if (action =="D"  && level =="(AODVTYPE_MY_DATA)" )
{
arr[$20] = 1 # $20 is packet id, so store at index [1 or index [2 the value equal to 1 and then at the end calculate total 1's in all                   			#indexes

};

if (action =="s"  && level =="(AODVTYPE_MY_DATA)" && source ==src1 && Agent == "RTR")
{
t_arr = time;
flag1 =1;
sent_packets1++;
  retransmissions = 0;


};

if (action =="r"  && level =="(AODVTYPE_MY_DATA)" && destination == dst1 && Agent == "MAC" )
{


      if (flag1 == 1)
	  {


	  received_packets1++;
	  delay = time - t_arr;
	  total_delay1 = total_delay1 + delay;
       total_retransmit= total_retransmit + retransmissions;
       print " Retransmit = "  retransmissions; 
	  
	  hops = 256 - ttl;
	  total_hops += hops;
	  flag1 = 0;
	 # print "start time =" t_arr " end time = " time  " delay  =" delay  " total delay = " total_delay;
	  };

	  };
 
}





END{
print "Sent " sent_packets1;
  print "received packets " received_packets1;

for (i=1; i <= 50; i++) 
{
total_drops = total_drops + arr["["i];
}
        
#print "droped " total_drops;


avg_delay = total_delay1/received_packets1;
avg_retransmissions = total_retransmit/received_packets1;

print "Average end-to-end transmission delay is " avg_delay*1000  " ms";
print "data delivery ratio " received_packets1/sent_packets1*100;
print "Average retransmissions  " avg_retransmissions  " vehicles";



  
  
  

#print "Average intermeeting time  " avg_delay/avg_retransmissions*1000  " ms";
   
  #print "Average number of hops is " total_hops/num_samples " seconds";
#print "Sent " sent_packets;
 # print "Sample " num_samples;

  };