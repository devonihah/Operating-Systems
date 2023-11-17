
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	// Retrieve environment variables
	char* contentLengthStr = getenv("CONTENT_LENGTH");
	char* queryString = getenv("QUERY_STRING");

	// Convert CONTENT_LENGTH to an integer
	int contentLength = (contentLengthStr != NULL) ? atoi(contentLengthStr) : 0;

	// Allocate memory for the request body buffer
	char* requestBody = (char*)malloc(contentLength + 1);

	// Read exactly CONTENT_LENGTH bytes from standard input
	fread(requestBody, 1, contentLength, stdin);

	// Null-terminate the request body
	requestBody[contentLength] = '\0';

	// Create the response body
	char responseBody[1024];
	sprintf(responseBody, "Hello CS324\nQuery string: %s\nRequest body: %s\n", queryString, requestBody);

	// Calculate the total length of the response body
	int totalLength = strlen(responseBody);

	// Send headers
	printf("Content-Type: text/plain\r\n");
	printf("Content-Length: %d\r\n", totalLength);
	printf("\r\n");  // End of headers

	// Send the response body
	printf("%s", responseBody);

	// Free allocated memory
	free(requestBody);

	return 0;
}

