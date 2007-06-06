#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <list>
#include <string>
#include <utility>
using namespace std;


void parse(char *module, char *path)
{
  list<pair<string, string> > hookers;

  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    fprintf(stderr, "Error opening file %s: %s\n", path, strerror(errno));
    exit(-5);
  } 
  char buf[2048], *s;
  int line = 0;
  hookers.clear();
  while ((s = fgets(buf, 2047, f)) != NULL)
  {
           line++;
           char *where = strstr(buf, "@on ");
            line++;
            if (where != NULL)
            {
                char *wp = strchr(where, '(');
                if (wp != NULL)
                {
                   *wp = 0;
                   char *hookname = strdup(where+4);
		   string hookn = hookname;
		   char *pp = hookname;
		   while (*pp)
		   {
                        if (*pp == ':')
			    *pp = '_';
			pp++;
                   }
		   *wp = '(';
		   string fp = module;
		   fp += '_';
		   fp += hookname;

                   printf("static bool %s_%s%s", module, hookname, wp);
		   pair<string, string> hp(hookn, fp);
		   hookers.push_back(hp);
	        }
            }
            else
              printf("%s", s);
  }

  printf("\nstatic void %s_registerHookers()\n", module);
  printf("{\n");
  list<pair<string, string> >::iterator it = hookers.begin();
  while (it != hookers.end())
  {
	printf("\t%s_registerHooker(", (*it).first.c_str());
	printf("%s);\n", (*it).second.c_str());
        it++;
  }
  printf("}\n");
  printf("\nstatic void %s_deregisterHookers()\n", module);
  printf("{\n");
  it = hookers.begin();
  while (it != hookers.end())
  {
	printf("\t%s_deregisterHooker(", (*it).first.c_str());
	printf("%s);\n", (*it).second.c_str());
        it++;
  }
  printf("}\n");
  printf("\nextern \"C\" void %s_init(int isReload)\n", module);
  printf("{\n");
  printf("\t%s_registerHookers();\n", module);
  printf("\t%s_Module::onInit(isReload);\n", module);
  printf("}\n");
  printf("\nextern \"C\" void %s_unload()\n", module);
  printf("{\n");
  printf("\t%s_Module::onUnload();\n", module);
  printf("\t%s_deregisterHookers();\n", module);
  printf("}\n");
  printf("\nextern \"C\" void %s_reload()\n", module);
  printf("{\n");
  printf("\t%s_Module::onReload();\n", module);
  printf("\t%s_deregisterHookers();\n", module);
  printf("}\n");
}

int main(int argc, char **argv)
{
  parse(argv[1], argv[2]);
  return 0;
}
