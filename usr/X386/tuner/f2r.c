/* f2r Frame to refresh converter */
/* 1993 Nye Liu	*/

#include <stdio.h>

#define RoundTo8(x) ((int)(x) & ~((int)7))

main(int argc, char *argv[])
{
  char *mode, *temp;
  float ClockFreq;
  float HorizontalRefresh, VerticalRefresh;
  int   HorizontalFrame, VerticalFrame; 

  for (mode=argv[0]+strlen(argv[0])-1; mode!=argv[0] && *(mode-1)!='/'; mode--);
  printf("%s\n",mode);

  printf("VGA Clock Rate (MHz) ? ");
  scanf("%f",&ClockFreq);
  ClockFreq *= 1e6;
  if(strcmp(mode,"r2f")==0) {
      printf("Enter desired H refresh (kHz): ");
      scanf("%f",&HorizontalRefresh);
      HorizontalFrame=RoundTo8(ClockFreq/HorizontalRefresh/1e3);
      printf("Horizontal Frame: %d\n", HorizontalFrame);

      printf("Enter desired V refresh (Hz): ");
      scanf("%f",&VerticalRefresh);
      printf("Vertical Frame: %d\n",
	(int)(ClockFreq/HorizontalFrame/VerticalRefresh));
  }
  else {
      printf("Enter HF: ");
      scanf("%d",&HorizontalFrame);
      HorizontalFrame=RoundTo8(HorizontalFrame);
      printf("Horizontal refresh rate: %.1fkHz\n",
	ClockFreq/HorizontalFrame/1e3);

      printf("Enter VF: ");
      scanf("%d",&VerticalFrame);
      printf("Vertical refresh rate: %.1f Hz\n",
	ClockFreq/HorizontalFrame/VerticalFrame);
  }
}
