

#include <stdio.h>




void mac_btoa(char *bytes, char *alphanumeric)
{
	sprintf(alphanumeric,
		"%02x:%02x:%02x:%02x:%02x:%02x",
		bytes[0], bytes[1], bytes[2],
		bytes[3], bytes[4], bytes[5]
		);
}
void mac_atob(char *alphanumeric, char *bytes)
{
	/* don't know how to scanf and demote down
	 * to 8 bits */
	int ints[6];

	sscanf(alphanumeric, "%x:%x:%x:%x:%x:%x",
		&ints[0], &ints[1], &ints[2],
		&ints[3], &ints[4], &ints[5]
		);

	bytes[0] = ints[0]; bytes[1] = ints[1];
	bytes[2] = ints[2]; bytes[3] = ints[3];
	bytes[4] = ints[4]; bytes[5] = ints[5];


}


