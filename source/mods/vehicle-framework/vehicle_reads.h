#ifndef MOON_VEHICLE_LOCAL_READS_H
#define MOON_VEHICLE_LOCAL_READS_H
/* Feature-local checked reads until the canonical SDK owner supplies the
 * coordinated gameplay/reflection/Unity repair. No shared runtime ABI changes. */
static inline int vehicle_read_value(PTR box,void *out,U64 size,int *ok){
 if(!ok)return 0;
 PTR data=*ok&&box?unbox(box):0;
 if(!data||!out||!size){*ok=0;return 0;}
 dcopy(out,data,size);return 1;
}
static inline int vehicle_read_flag(PTR box,int *ok){
 unsigned char value=0;
 if(!vehicle_read_value(box,&value,sizeof(value),ok)||value>1){if(ok)*ok=0;return 0;}
 return value;
}
static inline int vehicle_read_boolean(PTR method_id,PTR self,int *ok){
 if(!ok||!*ok)return 0;
 return vehicle_read_flag(call(method_id,self,0,ok),ok);
}
static inline int vehicle_read_integer(PTR method_id,PTR self,int *ok){
 int value=0;if(ok&&*ok)vehicle_read_value(call(method_id,self,0,ok),&value,sizeof(value),ok);
 return value;
}
static inline float vehicle_read_number(PTR method_id,PTR self,int *ok){
 float value=0;if(ok&&*ok)vehicle_read_value(call(method_id,self,0,ok),&value,sizeof(value),ok);
 return value;
}
static inline DVec vehicle_read_vector(PTR method_id,PTR self,int *ok){
 DVec value={0};if(ok&&*ok)vehicle_read_value(call(method_id,self,0,ok),&value,sizeof(value),ok);
 return value;
}
static inline int vehicle_read_deadline(float now,float interval,float *deadline){
 if(!deadline||!(now>=0&&now<=3.402823466e38F)||!(interval>0&&interval<=3.402823466e38F))return 0;
 float next=now+interval;
 if(!(next>now&&next<=3.402823466e38F))return 0;
 *deadline=next;return 1;
}
#endif
