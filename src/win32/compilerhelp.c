/*
 *
 * A helper program for the compilation process
 *
 */

/* x,y,z,w 
 * | | | `-- private build
 * | | `---- release build
 * | `------ minor version
 * `-------- major version
 */

#include <stdio.h>

void main(int argc,char *argv[])
{
	FILE *openme;
	char inbuf[512];
	int i,pb=0,rb=0,mi=0,ma=0;

	if (argc == 1)
		exit(-1);

	if ((openme = fopen(argv[1],"r+"))==NULL)
	{
		printf("error\n");
		exit(-1);
	}

	fscanf(openme,"%s %s %d\n",inbuf,inbuf,&pb);		/*Read Buffer*/
	fscanf(openme,"%s %s %d\n",inbuf,inbuf,&rb);
	fscanf(openme,"%s %s %d\n",inbuf,inbuf,&mi);
	fscanf(openme,"%s %s %d\n",inbuf,inbuf,&ma);

	pb++;
	if (argc > 2)
	if (atoi(argv[2])==0)  /*Public Build*/
		rb++;

	printf("new version = %d,%d,%d,%d",ma,mi,rb,pb);

	rewind(openme);

	fprintf(openme,"#define pb %d\n",pb);		/*Write Buffer*/
	fprintf(openme,"#define rb %d\n",rb);
	fprintf(openme,"#define mi %d\n",mi);
	fprintf(openme,"#define ma %d\n",ma);

	fprintf(openme,"#define vFILEVERSION ma,mi,rb,pb\n#define vPRODUCTVERSION ma,mi,0,0\n#define vDISPFILEVERSION \"%d,%d,%d,%d\\0\"\n#define vSUBBUILD \"%d\\0\"\n",ma,mi,rb,pb,pb);

	fclose(openme);


}
