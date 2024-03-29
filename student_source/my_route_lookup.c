#include "io.h"
#include "utils.h"
#include "my_route_lookup.h"
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>


#define TBL24_SIZE 16777216
#define BLOCK_SIZE 256
#define CTE 1000
#define MISS 0

int main(int argc, char *argv[]) {
	int lectura = 0;
	char * routing_file= basename(argv[2]);
	char * input_file = basename(argv[2]);
	if (argc != 3) {
		printf("Uso: <FIB><InputPacketFile> %s, %s\n", routing_file, input_file);
		printf("<FIB>: name of an ASCII file containing the FIB (Forwarding Information Base)\n") ;
		printf("<InputPacketFile>:  name of the ASCII file containing a list of destination IP addresses to be processed, separated by new line characters  \n");
		exit(-1);
	}

	if(initializeIO(argv[1], argv[2]) != 0){
		perror("initializeIO");
		printf("Not able to read the tables\n");
		return 0;
	}

	uint16_t * tbl24 = (uint16_t *)calloc(TBL24_SIZE, sizeof(uint16_t));
	uint16_t * tblong = (uint16_t *)malloc(0);
	if(tbl24 == NULL || tblong == NULL){
		perror("malloc");
		return 0;
	}

	int n_options, prefixLength,outInterface, i, aux, nblocks_tblong = 0;
	uint32_t tbl24_position = 0;
	uint16_t tblong_position, table_entry = 0;
	uint32_t prefix = 0;




	while(lectura != EOF){
		lectura =  readFIBLine(&prefix, &prefixLength, &outInterface);
		//printf("\n>> Input: %d/%d >> %d\n", prefix, prefixLength, outInterface);
		if(lectura != 0){
		  perror("readFIBLine");
		  break;
		}
		tbl24_position = prefix >> 8; // get rid of the 8 last bits we dont want (/24)


		if(prefixLength <=  24){

			table_entry = (uint16_t) outInterface;
			n_options = pow(2, 24-prefixLength);
			for(i = 0; i < n_options; i++){
			memcpy(&tbl24[tbl24_position + i], &table_entry, sizeof(uint16_t));
			}
			//printf("\tADDED TBL24 in Range[%d , %d] >> %d\n",tbl24_position, tbl24_position + n_options, tbl24[tbl24_position] );

		}  else if(prefixLength > 24){


			if ( (tbl24[tbl24_position] & (uint16_t) pow(2,15)) != (uint16_t)0){ /* Relacion ya existente en TBL24*/

				/* Obteniendo referencia de TBL24 a TBLong > (1002 - 1000) *256 = 2*256 = 512 [Posicion memoria]*/
				tbl24_position = ((tbl24[tbl24_position] ^ (uint16_t) pow(2,15)) - CTE) * BLOCK_SIZE;

				/* Indice dentro de TBLong y n_opciones*/
				tblong_position = (uint16_t) (prefix & 0xFF);
				n_options = pow(2, 32-prefixLength);
				table_entry = (uint16_t)outInterface;

				/* Escritura en las posiciones de memoria  de TBLong*/
				for(i = 0; i < n_options; i++ ){
					memcpy(&tblong[(tbl24_position + tblong_position) + i], &table_entry, sizeof(uint16_t));
				}

				//printf("\tMOD TBLong in MemoryRange[%d , %d] >> %d\n", (tbl24_position + tblong_position), (tbl24_position  + tblong_position + n_options), table_entry );

			}else{ /* Nueva relacion a TBLong */

				/* Guardamos el valor del prefijo /24 para ponerlo por defecto */
				aux = tbl24[tbl24_position];

				/*Escribo un 1 al inicio mas una referencia a TBLong en TBL24 */
				table_entry = CTE + nblocks_tblong;
				table_entry = (uint16_t)(table_entry | (uint16_t) pow(2,15));
				memcpy(&tbl24[tbl24_position], &table_entry, sizeof(uint16_t));
				tbl24_position = (nblocks_tblong * BLOCK_SIZE);

				/*Nueva posicion al ultimo octeto y posibilidades */
				tblong_position = prefix & (uint16_t) 0xFF;
				n_options = pow(2, 32-prefixLength);

				/*Aumentamos el tamaño de la memoria en un bloque de 256*/
				tblong = realloc(tblong, BLOCK_SIZE * (++nblocks_tblong) * sizeof(uint16_t));

				if(tblong == NULL){
					perror("realloc");
				}

				for(i = 0; i < BLOCK_SIZE; i++){
					memcpy(&tblong[tbl24_position+i], &aux, sizeof(uint16_t));
				}

				table_entry = (uint16_t) outInterface;
				for(i = 0; i<n_options; i++ ){
					memcpy(&tblong[tbl24_position + tblong_position + i], &table_entry, sizeof(uint16_t));
				}
				//printf("\tNEW TBLong [DEF=%d] in MemoryRange[%d , %d] >> %d\n", aux, (tbl24_position + tblong_position) ,(tbl24_position + tblong_position + n_options) , outInterface );
			}

		}else{
			puts("Reading prefix length failed");
			break;
		}

	}


	uint32_t IPAddress;
  float numberIpProcessed = 0;
  float numberOfTableAccesses = 0;
	int totalTableAccesses = 0;
	float AvgTableAccessed = 0;
  struct timespec initialTime;
  struct timespec finalTime;
  double searchingTime = 0;
	double totalSearchingTime = 0;
	double AvgSearchingTime = 0;
  lectura = 0;


  while(lectura == OK){
    lectura = readInputPacketFileLine(&IPAddress);
    if(lectura != 0){
      perror("readInputPacketFileLine");
      break;
    }

		numberIpProcessed++;
		numberOfTableAccesses = 0;
    tbl24_position = IPAddress >> 8;


		/*	table_position = table_position >>16;
			printf("**************************%d.", IPAddress);*/

		clock_gettime(CLOCK_REALTIME, &initialTime);
		if(tbl24[tbl24_position] == (uint16_t) 0){

			outInterface = MISS;
      numberOfTableAccesses++;


    }else if( (tbl24[tbl24_position] & (uint16_t) pow(2,15)) != (uint16_t) 0) {


			tblong_position = (((tbl24[tbl24_position] ^ (uint16_t) pow(2,15)) - CTE) * BLOCK_SIZE ) + (uint16_t)(IPAddress & 0xFF);
			outInterface = tblong[tblong_position];
			numberOfTableAccesses =+ 2;


    }else{

      outInterface = tbl24[tbl24_position];
      numberOfTableAccesses++;
    }
		clock_gettime(CLOCK_REALTIME, &finalTime);

		printOutputLine(IPAddress, outInterface, &initialTime, &finalTime, &searchingTime, numberOfTableAccesses);
		totalTableAccesses = totalTableAccesses + numberOfTableAccesses;
		totalSearchingTime = totalSearchingTime + searchingTime;

  }
	AvgTableAccessed = totalTableAccesses/numberIpProcessed;
	AvgSearchingTime = totalSearchingTime/numberIpProcessed;
	printSummary(numberIpProcessed, AvgTableAccessed, AvgSearchingTime);


	free(tbl24);
	free(tblong);
	freeIO();
	return 0;
}
