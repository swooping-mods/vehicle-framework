#ifndef RUSTBOUND_SCENE_ADAPTER_H
#define RUSTBOUND_SCENE_ADAPTER_H
/* Small scene boundary kept separate so ownership/error paths can be checked
 * with a host-side object graph without starting a game runtime. */
static inline int rv_layer(PTR go,int layer){int ok=1;if(layer<0||layer>31)return 0;void *a[]={&layer};call(scene_set_layer,go,a,&ok);return ok;}
static inline PTR rv_child(const char *name,PTR parent){
    int ok=1,layer=parent?integer(scene_get_layer,rv_object(parent),&ok):0;if(!ok)return 0;
    PTR go=su_gameobject(name,parent);if(go&&!rv_layer(go,layer)){su_destroy(go);return 0;}return go;
}
#endif
