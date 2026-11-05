#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "summary.h"
#include "display.h"
void clean_fact_text(const char *src,char *out,size_t cap){
 size_t n=0;unsigned parens=0,brackets=0;bool space=false;
 for(size_t i=0;src[i]&&n+5<cap;){
  unsigned char c=(unsigned char)src[i];
  if(c=='('){parens++;i++;continue;}if(c==')'){if(parens)parens--;i++;space=true;continue;}
  if(c=='['){brackets++;i++;continue;}if(c==']'){if(brackets)brackets--;i++;continue;}
  if(parens||brackets){i++;continue;}
  if(c<128&&isspace(c)){space=true;i++;continue;}
  if(c==0xc2&&(unsigned char)src[i+1]==0xa0){space=true;i+=2;continue;}
  if(c==0xe2&&(unsigned char)src[i+1]==0x80&&src[i+2]){
   unsigned char last=(unsigned char)src[i+2];
   if(last==0xaf){space=true;i+=3;continue;}
   if(last==0x93||last==0x94||last==0x98||last==0x99||last==0x9c||last==0x9d){
    if(space&&n&&out[n-1]!=' '){out[n++]=' ';}
    space=false;
    out[n++]=(last==0x93||last==0x94)?'-':(last==0x98||last==0x99)?'\'':'"';i+=3;continue;
   }
  }
  if(space&&n&&out[n-1]!=' '&&c!=','&&c!='.'&&c!=';'&&c!=':')out[n++]=' ';
  space=false;size_t bytes=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;
  bool valid=true;for(size_t k=1;k<bytes;k++)if(!src[i+k]||((unsigned char)src[i+k]&0xc0)!=0x80){valid=false;break;}
  if(!valid)break;
  memcpy(out+n,src+i,bytes);n+=bytes;i+=bytes;
 }
 while(n&&out[n-1]==' '){n--;}
 out[n]=0;
}
static bool abbreviation(const char *start,const char *dot){
 const char *word=dot;while(word>start&&(isalpha((unsigned char)word[-1])||word[-1]=='.'))word--;
 size_t n=(size_t)(dot-word);const char *short_words[]={"St","Mt","Dr","Mr","Mrs","Ms","Jr","Sr","vs","ca","e.g","i.e","U.S","U.K"};
 if(n==1&&isupper((unsigned char)*word))return true;
 for(size_t i=0;i<sizeof(short_words)/sizeof(short_words[0]);i++)if(strlen(short_words[i])==n&&!strncmp(word,short_words[i],n))return true;
 return false;
}
bool summarize_extract(const char *title,const char *extract,char *out,size_t capacity){
 char clean[2048],sentence[768],candidate[768];clean_fact_text(extract,clean,sizeof(clean));out[0]=0;
 const char *start=clean;unsigned used=0;
 for(const char *p=clean;*p;p++){
  if(*p!='.'&&*p!='!'&&*p!='?')continue;
  if(p[1]&&p[1]!=' '&&p[1]!='\n'&&p[1]!='"')continue;
  if(*p=='.'&&(abbreviation(start,p)||(p>clean&&p[-1]=='.')||p[1]=='.'))continue;
  size_t n=(size_t)(p-start+1);if(n>=sizeof(sentence)){start=p+1;while(*start==' ')start++;continue;}
  memcpy(sentence,start,n);sentence[n]=0;
  if(!out[0])snprintf(candidate,sizeof(candidate),"%s",sentence);
  else if(strlen(out)+1+n<sizeof(candidate)){size_t used_bytes=strlen(out);memcpy(candidate,out,used_bytes);candidate[used_bytes]=' ';memcpy(candidate+used_bytes+1,sentence,n+1);}
  else break;
  if(strlen(candidate)<capacity&&display_summary_fits(title,candidate)){
   strcpy(out,candidate);if(++used==2)break;
  }else if(out[0])break;
  start=p+1;while(*start==' '||*start=='"')start++;
 }
 return out[0]!=0;
}
