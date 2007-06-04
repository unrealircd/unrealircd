#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <list>
#include <string>

using namespace std;

list<string>& analyze(char *wp)
{
	static list<string> ret;
        char *wppp = wp+1;
        string s = wppp;
        string::size_type l = s.length();
        int bracketcount = 0;
        int state = 0;
	ret.clear();
        for (string::size_type i = 0; i < l; i++)
        {
                if (s[i] == '<')
                {
                        bracketcount++;
                        state = 1;
                }
                else if (s[i] == '>')
                        bracketcount--;
                if (state == 1 && s[i] == ',')
                        s[i] = '/';
                if (bracketcount == 0)
                        state = 0;
        }
	s[l-1] = ',';
	
	for (string::size_type i = s.find_first_of(','); i != string::npos; i = s.find_first_of(',', i+1))
	{
		string::size_type p = s.find_last_of(" &*(", i);
		string func = s.substr(p+1, i-p-1);
		ret.push_back(func);
	}
	return ret;
}


void parse(char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    fprintf(stderr, "Error opening file %s: %s\n", path, strerror(errno));
    exit(-5);
  } 
  char buf[2048], *s;
  int line = 0;
  while ((s = fgets(buf, 2047, f)) != NULL)
  {
     line++;
           char *where = strstr(buf, "@hook ");
            if (where != NULL)
            {
                char *wp = strchr(where+6, '(');
                if (wp != NULL)
                {
                   *wp = 0;
                   char *hookname = strdup(where+6);
                   *wp = '(';
                   char *wpe = strchr(wp, ';');
                   if (wpe != NULL)
                   {
                       *wpe = 0;
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\ttypedef bool (*%s_hooker)%s;\n\n", hookname, wp);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s%s\n", hookname, wp);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\tstd::list<%s_hooker>::iterator it = %s_hookers().begin();\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\twhile (it != %s_hookers().end())\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\tif ((*(*it))(");
	               list<string>& args = analyze(wp);
		       list<string>::iterator it = args.begin();
		       while (it != args.end())
	               {
				string arg = (*it);
				printf("%s", arg.c_str());
				it++;	
				if (it != args.end())
					printf(",");			
		       }		       
		       printf("))\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t\tit++;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\telse break;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic std::list<%s_hooker>& %s_hookers()\n\t{\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\tstatic std::list<%s_hooker> hookers;\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\treturn hookers;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s_registerHooker(%s_hooker hooker)\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n\t\t%s_hookers().push_front(hooker);\n\t}\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s_deregisterHooker(%s_hooker hooker)\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
		       printf("\t\t%s_hookers().remove(hooker);\n\t}\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
 
                   }
                   free(hookname);
                }
                continue;
            }
            else if ((where = strstr(buf, "@rhook ")) != NULL)
            {
                char *wp = strchr(where+7, '(');
                if (wp != NULL)
                {
                   *wp = 0;
                   char *hookname = strdup(where+7);
                   *wp = '(';
                   char *wpe = strchr(wp, ';');
                   if (wpe != NULL)
                   {
                       *wpe = 0;
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\ttypedef bool (*%s_hooker)%s;\n\n", hookname, wp);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s%s\n", hookname, wp);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\tstd::list<%s_hooker>::iterator it = %s_hookers().begin();\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\twhile (it != %s_hookers().end())\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\tif ((*(*it))(");
	               list<string>& args = analyze(wp);
		       list<string>::iterator it = args.begin();
		       while (it != args.end())
	               {
				string arg = (*it);
				printf("%s", arg.c_str());
				it++;	
				if (it != args.end())
					printf(",");			
		       }		       
		       printf("))\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t\tit++;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t\telse break;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic std::list<%s_hooker>& %s_hookers()\n\t{\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\tstatic std::list<%s_hooker> hookers;\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t\treturn hookers;\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t}\n");
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s_registerHooker(%s_hooker hooker)\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n\t\t%s_hookers().push_back(hooker);\n\t}\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\tstatic void %s_deregisterHooker(%s_hooker hooker)\n", hookname, hookname);
		       printf("#line %i \"%s\"\n", line, path);
                       printf("\t{\n");
		       printf("#line %i \"%s\"\n", line, path);
		       printf("\t\t%s_hookers().remove(hooker);\n\t}\n", hookname);
		       printf("#line %i \"%s\"\n", line, path);
                   }
                   free(hookname);
                   }
                   continue;
            }
            else
            {
              printf("#line %i \"%s\"\n%s", line, path, s);
            }
  }
}

int main(int argc, char **argv)
{
  parse(argv[1]);
  return 0;
}
