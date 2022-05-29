/*************************************************************************************************/
// Preprocessor
/*************************************************************************************************/
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#include <time.h>
#include <cute.h>
using namespace cute;
#define CUTE_PATH_IMPLEMENTATION
#include <cute/cute_path.h>
#include <sokol/sokol_gfx_imgui.h>
#include <imgui/imgui.h>

#define ENG_DICT_LINES 279498
#define MAX_WORD_LEN 15
#define MAX_DICT_WORD_LEN 20
#define PILE_DIM 4
#define MAX_PILE_SIZE PILE_DIM*PILE_DIM

/*************************************************************************************************/
// Global data
/*************************************************************************************************/
uint16_t port = 5001;
uint64_t appID = 234;

batch_t* batch_p;
sprite_t letter_sprites[26];
sprite_t letter_back;

rnd_t rnd;

dictionary<string_t, array<string_t>> fastDict;
char engdict_words[ENG_DICT_LINES][MAX_DICT_WORD_LEN];
//hashtable_t engdict_words;

//#define CLIENT
//#define SERVER

// both client and server use these
char pileBuf[MAX_PILE_SIZE] = { '\0' };
int pileBufFlags[MAX_PILE_SIZE] = { 0 };
char pileSorted[MAX_PILE_SIZE + 1] = { '\0' };
int pileSortedIndices[MAX_PILE_SIZE + 1] = { -1 };
int pileFaceupCount = 0;
enum pileTileState
{
	empty,
	facedown,
	faceup
};

//#ifdef CLIENT
client_t* client_p;
uint32_t client_id;

// letters typed by a player
char letterBuf[MAX_WORD_LEN+1] = {0};
//#endif

//#ifdef SERVER
server_t* server;
//#endif

// Embedded g_public_key
int g_public_key_sz = 32;
unsigned char g_public_key_data[32] = {
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};
// Embedded g_secret_key
int g_secret_key_sz = 64;
unsigned char g_secret_key_data[64] = {
	0x10,0xaa,0x98,0xe0,0x10,0x5a,0x3e,0x63,0xe5,0xdf,0xa4,0xb5,0x5d,0xf3,0x3c,0x0a,
	0x31,0x5d,0x6e,0x58,0x1e,0xb8,0x5b,0xa4,0x4e,0xa3,0xf8,0xe7,0x55,0x53,0xaf,0x7a,
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};

/*************************************************************************************************/
uint64_t unix_timestamp();
bool is_a_word(const char* word);
void sortPile();
bool canPileSteal(const char* str);

/*************************************************************************************************/
error_t make_test_connect_token(uint64_t unique_client_id, const char* address_and_port, uint8_t* connect_token_buffer)
{
	crypto_key_t client_to_server_key = crypto_generate_key();
	crypto_key_t server_to_client_key = crypto_generate_key();
	uint64_t current_timestamp = unix_timestamp();
	uint64_t expiration_timestamp = current_timestamp + 60; // Token expires in one minute.
	uint32_t handshake_timeout = 5;
	const char* endpoints[] = {
		address_and_port,
	};

	uint8_t user_data[CUTE_CONNECT_TOKEN_USER_DATA_SIZE];
	memset(user_data, 0, sizeof(user_data));

	error_t err = generate_connect_token(
		appID,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		unique_client_id,
		user_data,
		(crypto_sign_secret_t*)g_secret_key_data,
		connect_token_buffer
	);

	return err;
}
void deleteLastTypedChar()
{
	if (strlen(letterBuf))
		letterBuf[strlen(letterBuf) - 1] = '\0';
}
void update_letterBuf()
{
	int cap_diff = 'a' - 'A';
	for(int i='a';i<='z';++i)
	{
		if (key_was_pressed(key_button_t(i))) {
			if(strlen(letterBuf) < MAX_WORD_LEN)
			{
				char letter[2];
				letter[0] = i - cap_diff;
				letter[1] = '\0';
				strcat(letterBuf, letter);
			}
		}	
	}
	if (key_was_pressed(KEY_BACKSPACE))
		deleteLastTypedChar();
}
sprite_t* get_letter_sprite(char c)
{
	if(c >= 'A' && c <= 'Z')
	{
		return &letter_sprites[c - 'A'];
	}
	if(c >= 'a' && c <= 'z')
	{
		return &letter_sprites[c - 'a'];
	}
	return nullptr;
}
void render_string(const char* str, int spacing, v2 pos, float scale)
{
	float xoffs = pos.x;
	float yoffs = pos.y;
	for (int i = 0; i < strlen(str); i++)
	{
		sprite_t* spr = get_letter_sprite(str[i]);
		spr->transform.p.x = xoffs;
		spr->transform.p.y = yoffs;
		spr->scale.x = scale;
		spr->scale.y = scale;
		xoffs += spacing;
		spr->draw(batch_p);
		spr->scale.x = 1;
		spr->scale.y = 1;
	}
}
void client_update_code(float dt)
{	
	uint64_t unix_time = unix_timestamp();

	client_update(client_p, dt, unix_time);

	if (client_state_get(client_p) == CLIENT_STATE_CONNECTED) {
		static bool notify = false;
		if (!notify) {
			notify = true;
			printf("Connected! Press ESC to gracefully disconnect.\n");
		}

		if (key_was_pressed(KEY_RETURN)) {
			char data[50];
			strcpy(data,"kp:enter:");
			strcat(data, letterBuf);
			int size = (int)strlen(data) + 1;
			client_send(client_p, data, size, false);
		}
		if (key_was_pressed(KEY_ESCAPE)) {
			client_disconnect(client_p);
			app_stop_running();
		}
	} else if (client_state_get(client_p) < 0) {
		printf("Client encountered an error: %s.\n", client_state_string(client_state_get(client_p)));
		exit(-1);
	}
}
void server_update_code(float dt)
{
	static float flip_timer = 0;
	flip_timer += dt;
	if (flip_timer > (pileFaceupCount+1))
	{
		if (pileFaceupCount == MAX_PILE_SIZE)
		{
			printf("\nGG! Game is done!\n");
		}
		else
		{
			int index = rnd_next_range(rnd, 0, MAX_PILE_SIZE - 1);
			while (pileBufFlags[index] != pileTileState::facedown) { index = rnd_next_range(rnd, 0, MAX_PILE_SIZE - 1); }
			pileBufFlags[index] = pileTileState::faceup;
			flip_timer = 0;
			pileFaceupCount++;
			sortPile();
		}
	}

	// render the pile
	float yoffset =  5 * 64 + 20;
	for (int i = 0; i < PILE_DIM; i++)
	{
		float xoffset = -7 * 64 - 25;
		for (int j = 0; j < PILE_DIM; j++)
		{
			if (pileBufFlags[i * PILE_DIM + j] == pileTileState::faceup)
			{
				int letter_index = pileBuf[i * PILE_DIM + j] - 'A';
				letter_sprites[letter_index].transform.p.x = xoffset;
				letter_sprites[letter_index].transform.p.y = yoffset;
				letter_sprites[letter_index].draw(batch_p);
			}
			else if (pileBufFlags[i * PILE_DIM + j] == pileTileState::facedown)
			{
				letter_back.transform.p.x = xoffset;
				letter_back.transform.p.y = yoffset;
				letter_back.draw(batch_p);
			}
			xoffset += 64;
		}
		yoffset -= 64;
	}

	// each player
	render_string("PLAYERONE"  , letter_sprites[0].w / 4, v2(4 * 64, 11 * 32), 0.3);
	render_string("PLAYERTWO"  , letter_sprites[0].w / 4, v2(4 * 64, 7 * 32), 0.3);
	render_string("PLAYERTHREE", letter_sprites[0].w / 4, v2(4 * 64, 4 * 32), 0.3);
	render_string("PLAYERFOUR" , letter_sprites[0].w / 4, v2(4 * 64, 1 * 32), 0.3);

	batch_flush(batch_p);

	uint64_t unix_time = unix_timestamp();

	server_update(server, dt, unix_time);

	server_event_t e;
	while (server_pop_event(server, &e)) {
		if (e.type == SERVER_EVENT_TYPE_NEW_CONNECTION) {
			printf("New connection from id %d, on index %d.\n", (int)e.u.new_connection.client_id, e.u.new_connection.client_index);
		}
		else if (e.type == SERVER_EVENT_TYPE_PAYLOAD_PACKET) {
			const char* msg = (const char*)e.u.payload_packet.data;
			printf("Got a message from client on index %d, \"%s\"\n", e.u.payload_packet.client_index, msg);
			const char* msg_header = "kp:enter:";
			const int msg_header_len = strlen(msg_header);
			if (strcmp(msg, msg_header))
			{
				const char* word = (const char*)e.u.payload_packet.data + msg_header_len; // skip the header
				printf("A player is trying to make word: %s\n", word);
				if (is_a_word(word))
					printf("It is a word!\n");
				else
					printf("It is not a word...\n");
			}
			server_free_packet(server, e.u.payload_packet.client_index, e.u.payload_packet.data);
		}
		else if (e.type == SERVER_EVENT_TYPE_DISCONNECTED) {
			printf("Client disconnected on index %d.\n", e.u.disconnected.client_index);
		}
	}

	// for testing on server by yourself
	if (key_was_pressed(KEY_RETURN))
	{
		if (is_a_word(letterBuf))
		{
			printf("%s is a word!\n", letterBuf);
			if (canPileSteal(letterBuf))
			{
				printf("can pile steal!\n");
			}
			else
				printf("cannot pile steal...\n");
		}
		else
			printf("%s is not a word...\n", letterBuf);
	}
}
void main_loop()
{
	while (app_is_running())
	{
		float dt = calc_dt();
		app_update(dt);

		// typing
		update_letterBuf();
		float offset = -7 * 64;
		for (int i = 0; i < strlen(letterBuf); i++)
		{
			int letter_index = letterBuf[i] - 'A';
			letter_sprites[letter_index].transform.p.x = offset;
			letter_sprites[letter_index].transform.p.y = -300;
			offset += 64;
			letter_sprites[letter_index].draw(batch_p);
		}

		batch_flush(batch_p);

#ifdef CLIENT
		client_update_code(dt);
#endif

#ifdef SERVER
		server_update_code(dt);
#endif
		app_present();
	}
}
void panic(error_t err)
{
	printf("ERROR: %s\n", err.details);
	exit(-1);
}
uint64_t unix_timestamp()
{
	time_t ltime;
	time(&ltime);
	struct tm* timeinfo = gmtime(&ltime);;
	return (uint64_t)mktime(timeinfo);
}
void mount_content_folder()
{
	char buf[1024];
	const char* base = file_system_get_base_dir();
	path_pop(base, buf, NULL);
#ifdef _MSC_VER
	path_pop(buf, buf, NULL); // Pop out of Release/Debug folder when using MSVC.
#endif
	strcat(buf, "/assets");
	file_system_mount(buf, "");
}
void client_init_code()
{
	printf("Setting up Client");
	client_p = client_create(0, appID);
	
	// Must be unique for each different player in your game.
	uint64_t client_id = (uint64_t)rnd_next_range(rnd, 0, 9999999);
	printf("my client ID is: %d", (int)client_id);

	const char* server_address_and_port = "127.0.0.1:5001";
	endpoint_t endpoint;
	endpoint_init(&endpoint, server_address_and_port);

	uint8_t connect_token[CUTE_CONNECT_TOKEN_SIZE];
	error_t err = make_test_connect_token(client_id, server_address_and_port, connect_token);
	if (err.is_error()) panic(err);
	err = client_connect(client_p, connect_token);
	if (err.is_error()) panic(err);
}
void server_init_code()
{
	printf("Setting up Server");
	const char* address_and_port = "127.0.0.1:5001";
	endpoint_t endpoint;
	endpoint_init(&endpoint, address_and_port);

	server_config_t server_config = server_config_defaults();
	server_config.application_id = appID;
	memcpy(server_config.public_key.key, g_public_key_data, sizeof(g_public_key_data));
	memcpy(server_config.secret_key.key, g_secret_key_data, sizeof(g_secret_key_data));

	server = server_create(server_config);
	error_t err = server_start(server, address_and_port);
	if (err.is_error()) panic(err);
}
void load_eng_dict()
{
	const char* filedata;
	char filepath[100] = "wordlists/engdict.txt";

	size_t filesize = 0;
	file_system_read_entire_file_to_memory_and_nul_terminate(
		filepath,
		(void**)&filedata,
		&filesize
		);

	//hashtable_init(engdict_words, );

	const char* cur = filedata;
	const char* end = filedata + filesize;
	for(int i=0;i<ENG_DICT_LINES;i++)
	//for(int i=0;i<5;i++)
	{
		int linecount = 0;
		bool foundSpace = false;
		while(cur < end && *cur != '\n')
		{
			if (i >= 2) // skip first 2 lines
			{
				if (!foundSpace)
				{
					engdict_words[i-2][linecount] = *cur;
					//printf("\n%s", &);

					if (*cur == ' ')
					{
						foundSpace = true;
						engdict_words[i-2][linecount + 1] = '\0';
					}
				}
			}
			linecount++;
			cur++;
		}
		cur++; // skip \r
	}

	printf("\n%s", &engdict_words[ENG_DICT_LINES - 3][0]);
	printf("\n%s", &engdict_words[ENG_DICT_LINES - 2][0]);
	printf("\n%s", &engdict_words[ENG_DICT_LINES - 1][0]);
}
void load_sorted_word_list(uint32_t n)
{
	const char* filedata;
	char filepath[100] = "wordlists/";
	char onedigit[2];
	onedigit[0] = (n < 10) ? ('0'+n) : ('0');
	onedigit[1] = '\0';
	char twodigits[3];
	twodigits[0] = (n < 10) ? ('0') : ('0'+(n/10));
	twodigits[1] = (n < 10) ? ('0') : ('0'+(n%10));
	twodigits[2] = '\0';
	//printf("\nonedigit:  %s", onedigit);
	//printf("\ntwodigits: %s", twodigits);

	//printf("\nfilepath: %s", filepath);
	strcat(filepath, (n < 10) ? onedigit : twodigits);
	//printf("\nfilepath: %s", filepath);
	strcat(filepath, "sort.txt");
	
	printf("\nfilepath: %s", filepath);

	size_t filesize = 0;
	file_system_read_entire_file_to_memory_and_nul_terminate(
		filepath,
		(void**)&filedata,
		&filesize
		);
	
	printf("\nfilesize: %d", (int)filesize);

	const char* cur = filedata;
	const char* end = filedata + filesize;
	while(cur < end)
	{
		array<string_t>* arr = nullptr;
		
		string_t tempkey = string_t(cur, cur + n);
		cur += n;
		printf("\n1tempkey:%s\n", tempkey.c_str());
		tempkey.incref();

		cur++; // comma skip

		string_t tempvalue = string_t(cur, cur + n);
		cur += n + 2;
		printf("\n2tempvalue:%s", tempvalue.c_str());
		tempvalue.incref();

		arr = fastDict.find(tempkey);
		if(!arr)
		{
			int temp = fastDict.count();
			printf("\ninserting key #%d", temp);
			fastDict.insert(tempkey, {tempvalue});
		}
		else
		{
			printf("\nadding...");
			arr->add(tempvalue);
		}
	}

	printf("\nfastDict count: %d\n",fastDict.count());
	//printf("key: %s", fastDict.keys()[0].c_str());
	
	// for(int i=0;i<fastDict.count();i++)
	// {
	// 	printf("key: %s", fastDict.keys()[i].c_str());
	// 	for(int j=0;j<fastDict.items()[i].count();j++)
	// 	{
	// 		printf(", value: %s", fastDict.items()[i][j].c_str());
	// 	}
	// 	printf("\n");
	// }
}
void load_assets()
{
	// for(int i=3;i<4;i++)
	// {
	// 	load_sorted_word_list(i);
	// }

	load_eng_dict();

	// load sprites
	{
	letter_sprites[0 ] = sprite_make("art/letter_a.ase");
	letter_sprites[1 ] = sprite_make("art/letter_b.ase");
	letter_sprites[2 ] = sprite_make("art/letter_c.ase");
	letter_sprites[3 ] = sprite_make("art/letter_d.ase");
	letter_sprites[4 ] = sprite_make("art/letter_e.ase");
	letter_sprites[5 ] = sprite_make("art/letter_f.ase");
	letter_sprites[6 ] = sprite_make("art/letter_g.ase");
	letter_sprites[7 ] = sprite_make("art/letter_h.ase");
	letter_sprites[8 ] = sprite_make("art/letter_i.ase");
	letter_sprites[9 ] = sprite_make("art/letter_j.ase");
	letter_sprites[10] = sprite_make("art/letter_k.ase");
	letter_sprites[11] = sprite_make("art/letter_l.ase");
	letter_sprites[12] = sprite_make("art/letter_m.ase");
	letter_sprites[13] = sprite_make("art/letter_n.ase");
	letter_sprites[14] = sprite_make("art/letter_o.ase");
	letter_sprites[15] = sprite_make("art/letter_p.ase");
	letter_sprites[16] = sprite_make("art/letter_q.ase");
	letter_sprites[17] = sprite_make("art/letter_r.ase");
	letter_sprites[18] = sprite_make("art/letter_s.ase");
	letter_sprites[19] = sprite_make("art/letter_t.ase");
	letter_sprites[20] = sprite_make("art/letter_u.ase");
	letter_sprites[21] = sprite_make("art/letter_v.ase");
	letter_sprites[22] = sprite_make("art/letter_w.ase");
	letter_sprites[23] = sprite_make("art/letter_x.ase");
	letter_sprites[24] = sprite_make("art/letter_y.ase");
	letter_sprites[25] = sprite_make("art/letter_z.ase");
	letter_back        = sprite_make("art/letter_back.ase");
	}
}
bool is_a_word(const char* word)
{
	int wordlen = strlen(word);
	for (int i = 0; i < ENG_DICT_LINES; ++i)
	{
		for (int j = 0; j < MAX_WORD_LEN; ++j)
		{
			if (word[j] == engdict_words[i][j])
			{
				if (j == wordlen-1)
				{
					// make sure there is a null-terminator
					// or else it might be just the first part
					// of a word
					return engdict_words[i][j+1] == '\t';
				}
			}
			else
			{
				break;
			}
		}
	}
	return false;
}
void Shuffle(char* str)
{
	int n = strlen(str);
	while (n > 1)
	{
		n--;
		// random num between 0 and n-1
		int k = (int)rnd_next_range(rnd, 0, n-1);

		// swap with k with n
		char value = str[k];
		str[k] = str[n];
		str[n] = value;
	}
}
void init_game()
{
	for (int i = 0; i < MAX_PILE_SIZE; i++)
	{
		pileBuf[i] = '\0';
		pileBufFlags[i] = pileTileState::facedown;
		pileSorted[i] = '\0';
		pileSortedIndices[i] = -1;
	}

	// common letter distribution for popular word games
	//A - 9, B - 2, C - 2, D - 4, E - 12, F - 2, G - 3, H - 2, I - 9, J - 1, K - 1,
	//L - 4, M - 2, N - 6, O - 8, P - 2, Q - 1, R - 6, S - 4, T - 6, U - 4, V - 2,
	//W - 2, X - 1, Y - 2, Z - 1
	char letter_distr[100] = "AAAAAAAAABBCCDDDDEEEEEEEEEEEEFFGGGHHIIIIIIIIIJKLLLLMMNNNNNNNOOOOOOOOPPQRRRRRRSSSSTTTTTTUUUUVVWWXYYZ";
	Shuffle(letter_distr);
	for (int i = 0; i < MAX_PILE_SIZE; i++)
	{
		pileBuf[i] = letter_distr[i];
	}
	printf("\n%s", pileBuf);
}
char getPileVal(int i, int j)
{
	return pileBuf[i * PILE_DIM + j];
}
char* getSortedPile()
{
	return &pileSorted[MAX_PILE_SIZE - pileFaceupCount];
}
int sortCharsCompare(const void* a, const void* b)
{
	return (*(char*)a - *(char*)b);
}
void swapChars(char* xp, char* yp)
{
	char temp = *xp;
	*xp = *yp;
	*yp = temp;
}
void swapInts(int* xp, int* yp)
{
	int temp = *xp;
	*xp = *yp;
	*yp = temp;
}
void selectionSort(char arr[], int arr2[], int n)
{
	int i, j, min_idx;

	// One by one move boundary of unsorted subarray
	for (i = 0; i < n - 1; i++) {

		// Find the minimum element in unsorted array
		min_idx = i;
		for (j = i + 1; j < n; j++)
			if (arr[j] < arr[min_idx])
				min_idx = j;

		// Swap the found minimum element
		// with the first element
		swapChars(&arr[min_idx], &arr[i]);
		swapInts(&arr2[min_idx], &arr2[i]);
	}
}
void sortPile()
{
	for (int i = 0; i < MAX_PILE_SIZE; i++)
	{
		if (pileBufFlags[i] == pileTileState::faceup)
		{
			pileSorted[i] = pileBuf[i];
			pileSortedIndices[i] = i;
		}
		else
		{
			pileSorted[i] = '0';
		}
	}
	selectionSort(pileSorted, pileSortedIndices, MAX_PILE_SIZE);
	printf("sorted pile: %s\n", getSortedPile());
}
int findFirstFaceupChar(char c)
{
	for (int i = 0; i < MAX_PILE_SIZE; ++i)
	{
		if (pileBuf[i] == c && pileBufFlags[i] == pileTileState::faceup)
			return i;
	}
	return -1;
}
bool canPileSteal(const char* str)
{
	char word[MAX_WORD_LEN+1] = {'\0'};
	strcpy(word, str);
	int wordlen = strlen(word);
	qsort(word, wordlen, sizeof(char), sortCharsCompare);
	char* pile = getSortedPile();
	char* pile_end = pile + strlen(pile);

	// for each letter in the word
	char* word_p = word;
	for (int i = 0; i < wordlen; ++i)
	{
		// we should be able to increment through the sorted pile
		// and find all the letters in order

		// while the pile pointer is not null and
		// the chars are not equal
		while (pile < pile_end && *word_p != *pile)
		{
			pile++; // next pile char
		}
		if (pile < pile_end)
		{
			word_p++; // next word char
			pile++; // next pile char
		}
		else
		{
			return false;
		}
	}
	// set them to empty state
	for (int i = 0; i < wordlen; ++i)
	{
		int index = findFirstFaceupChar(word[i]);
		if (index >= 0)
			pileBufFlags[index] = pileTileState::empty;
		else
			printf("Error, char not found in pile\n");
	}
	printf("\n");
	for (int i = 0; i < wordlen; ++i) deleteLastTypedChar();
	pileFaceupCount -= wordlen;
	sortPile();
	return true;
}
int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app_make("Steal Words Multiplayer", 0, 0, 1500, 800, app_options, argv[0]);
	batch_p = sprite_get_batch();
	batch_set_projection(batch_p, matrix_ortho_2d(1024, 768, 0, 0));
	mount_content_folder();
	rnd = rnd_seed((uint64_t)time(0));

	app_init_imgui();
	init_game();
	load_assets();

#ifdef SERVER
	server_init_code();
#endif

#ifdef CLIENT
	client_init_code();
#endif

	main_loop();

	app_destroy();

	return 0;
}
