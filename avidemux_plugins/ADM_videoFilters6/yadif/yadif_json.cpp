// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "yadif.h"
bool  yadif_jserialize(const char *file, const yadif *key){
admJson json;
json.addUint32("mode",key->mode);
json.addInt32("parity",key->parity);
json.addUint32("deint",key->deint);
return json.dumpToFile(file);
};
bool  yadif_jdeserialize(const char *file, const ADM_paramList *tmpl,yadif *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
