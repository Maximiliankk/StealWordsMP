#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#include <time.h>
#include <cute.h>
using namespace cute;
#define CUTE_PATH_IMPLEMENTATION
#include <cute/cute_path.h>
#include <sokol/sokol_gfx_imgui.h>
#include <imgui/imgui.h>
#include <string>
#include <vector>
#define WINDOW_WIDTH 1900
#define WINDOW_HEIGHT 1200
#define ENG_DICT_LINES 279498
#define MAX_PLAYERS 8
#define MAX_WORDS_PER_PLAYER 10
#define MIN_WORD_LEN 4
#define MAX_WORD_LEN 15
#define MAX_DICT_WORD_LEN 20
#define PILE_DIM 10
#define MAX_WORDS_MADE_HISTORY 1000
#define MAX_PILE_SIZE PILE_DIM*PILE_DIM
#define TEST_DATA true
#define RENDERING_CODE false
#define DEBUG_PRINTS_NET true
#define DEBUG_PRINTS_PLAYER_WORDS false
#define SERVER_IP "64.225.77.115"
#define CLIENT_IP "104.156.104.108"
#define PORT "5001"
enum pileTileState
{
	empty,
	facedown,
	faceup
};
std::vector<char> pileBuf;
std::vector<char> pileBufFlags;
std::vector<uint32_t> playerNumWords;
std::vector<std::vector<std::string>> playerWords;
unsigned char numActivePlayers = 8;
std::vector<std::string> playerNames;
std::vector<std::string> wordsMade;
int numWordsMade = 0;
struct server_update_clients_packet
{
	char pileBuf[MAX_PILE_SIZE];
	char pileBufFlags[MAX_PILE_SIZE];
	//char player_words[MAX_PILE_SIZE * 2];
};
std::vector<char> pileSorted;
int pileFacedownIndices[MAX_PILE_SIZE];
int pileFaceupCount = 0;
std::vector<char> letterBuf;
char engdict_words[ENG_DICT_LINES][MAX_DICT_WORD_LEN];
//dictionary<string_t, array<string_t>> fastDict;
rnd_t rnd;
batch_t* batch_p;
sprite_t letter_sprites[26];
sprite_t letter_back;
uint16_t port = 5001;
uint64_t appID = 234;
client_t* client_p;
uint32_t client_id;
server_t* server;
int g_public_key_sz = 32;
unsigned char g_public_key_data[32] = {
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};
int g_secret_key_sz = 64;
unsigned char g_secret_key_data[64] = {
	0x10,0xaa,0x98,0xe0,0x10,0x5a,0x3e,0x63,0xe5,0xdf,0xa4,0xb5,0x5d,0xf3,0x3c,0x0a,
	0x31,0x5d,0x6e,0x58,0x1e,0xb8,0x5b,0xa4,0x4e,0xa3,0xf8,0xe7,0x55,0x53,0xaf,0x7a,
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};
uint64_t unix_timestamp();
bool is_a_word(const char* word);
void sortPile();
bool canPileSteal(const char* str);
void doPileSteal(int playerID, const char* word);
void tryAnagramSteal(const char* str, int playerID);
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
	if (strlen(&letterBuf[0]))
		letterBuf[strlen(&letterBuf[0]) - 1] = '\0';
}
void update_letterBuf()
{
	int cap_diff = 'a' - 'A';
	for(int i='a';i<='z';++i)
	{
		if (key_was_pressed(key_button_t(i))) {
			if(strlen(&letterBuf[0]) < MAX_WORD_LEN)
			{
				char letter[2];
				letter[0] = i - cap_diff;
				letter[1] = '\0';
				strcat(&letterBuf[0], letter);
			}
		}	
	}
	if (key_was_pressed(KEY_BACKSPACE))
	{
		int len = strlen(&letterBuf[0]);
		for (int i = 0; i < len; ++i)
			deleteLastTypedChar();
	}
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
	int len = strlen(str);
	for (int i = 0; i < len; i++)
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
void render_player_words()
{
	float player_data_spacing = 2.8;
	float halfw = (float)WINDOW_WIDTH / player_data_spacing, halfh = (float)WINDOW_HEIGHT / player_data_spacing;
	float smallhalf = halfw > halfh ? halfh : halfw;
	v2 pstart_pos[8] = {
		v2(-smallhalf, smallhalf),
		v2( smallhalf, smallhalf),
		v2(-smallhalf,-smallhalf),
		v2( smallhalf,-smallhalf),

		v2(-smallhalf, 0),
		v2( smallhalf, 0),
		v2( 0,         smallhalf),
		v2( 0,        -smallhalf),
	};

	float letter_scale = 0.45;
	int letter_width = (int)((float)letter_sprites[0].w / 2.5);
	v2 half_letterw(-letter_width/2, letter_width / 2);
	for (int i = 0; i < numActivePlayers; i++)
	{
		int pname_len = strlen(playerNames[i].c_str()) / 2;
		v2 half_max_words(0, MAX_WORDS_PER_PLAYER / 2 * letter_width);
		v2 half_player_name(pname_len * letter_width, 0);
		render_string(playerNames[i].c_str(), letter_width, pstart_pos[i] + half_letterw + half_max_words - half_player_name, letter_scale);
		for (int j = 0; j < playerNumWords[i]; j++)
		{
			int pword_len = strlen(&playerWords[i][j][0]) / 2;
			v2 word_row(0, -letter_width * (j + 1));
			v2 half_word(pword_len * letter_width, 0);
			render_string(&playerWords[i][j][0], letter_width, pstart_pos[i] + half_letterw + half_max_words + word_row - half_word, letter_scale);
		}
	}
}
void RemoveFacedownIndex(int index)
{
	for (int i = index; i < MAX_PILE_SIZE-1; i++)
		pileFacedownIndices[index] = pileFacedownIndices[index+1];
}
void render_pile()
{
	float letterscale = 0.7;
	int tilewidth = letter_sprites[0].w * letterscale;
	v2 pos(-PILE_DIM * tilewidth / 2,
		    PILE_DIM * tilewidth / 2);
	float xstart = pos.x;

	for (int i = 0; i < PILE_DIM; i++)
	{
		pos.x = xstart;
		for (int j = 0; j < PILE_DIM; j++)
		{
			if (pileBufFlags[i * PILE_DIM + j] == pileTileState::faceup)
			{
				int letter_index = pileBuf[i * PILE_DIM + j] - 'A';
				letter_sprites[letter_index].scale = v2(letterscale, letterscale);
				letter_sprites[letter_index].transform.p = pos;
				letter_sprites[letter_index].draw(batch_p);
			}
			else if (pileBufFlags[i * PILE_DIM + j] == pileTileState::facedown)
			{
				letter_back.scale = v2(letterscale, letterscale);
				letter_back.transform.p = pos;
				letter_back.draw(batch_p);
			}
			pos.x += tilewidth;
		}
		pos.y -= tilewidth;
	}
}
bool wordAlreadyMade(const char* word)
{
	for (int i = 0; i < numWordsMade; i++)
	{
		if (strcmp(word, wordsMade[i].c_str()) == 0 && strlen(word) == strlen(wordsMade[i].c_str()))
			return true;
	}
	return false;
}
void wordWasMade(const char* word)
{
	int wordlen = strlen(word);
	for (int i = 0; i < wordlen; i++)
		wordsMade[numWordsMade][i] = word[i];
	numWordsMade++;

#if DEBUG_PRINTS_PLAYER_WORDS == true
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		printf("............Player %d words................\n",i);
		for (int j = 0; j < MAX_WORDS_PER_PLAYER; j++)
		{
			for (int k = 0; k < MAX_WORD_LEN; k++)
			{
				//if (playerWords[j][k] == '\0')
					//continue;
				printf("%c", playerWords[i][j][k]);
			}
			printf("\n");
		}
	}
	printf("DDDDDDDDDDDDDD\n");
#endif
}
void client_update_code(float dt)
{	
	uint64_t unix_time = unix_timestamp();

	render_pile();

	client_update(client_p, dt, unix_time);

	if (client_state_get(client_p) == CLIENT_STATE_CONNECTED) {
		
		static float client_update_pkt_timer = 0;
		client_update_pkt_timer += dt;
		if (client_update_pkt_timer > 1)
		{
#if DEBUG_PRINTS_NET == true
			printf("Sending empty packet to server\n");
#endif
			char data = 'a';
			//client_send(client_p, (void*)&data, 1, false);
			error_t ret = client_send(client_p, nullptr, 0, false);
			if (ret.is_error())
			{
				printf("ERROR: %s\n", ret.details);
			}
			client_update_pkt_timer = 0;
		}

		static bool notify = false;
		if (!notify) {
			notify = true;
			printf("Connected! Press ESC to gracefully disconnect.\n");
		}

		if (key_was_pressed(KEY_RETURN)) {
			char data[50];
			strcpy(data,"kp:enter:");
			strcat(data, &letterBuf[0]);
			int size = (int)strlen(data) + 1;
			client_send(client_p, data, size, true);
		}
		if (key_was_pressed(KEY_ESCAPE)) {
			client_disconnect(client_p);
			app_stop_running();
		}

		void* p_data;
		int size;
		while (client_pop_packet(client_p, &p_data, &size))
		{
#if DEBUG_PRINTS_NET == true
			printf("Got a packet from server!\n");
#endif
			server_update_clients_packet sucp;
			memcpy(&sucp, p_data, size);
			memcpy(&pileBuf[0], sucp.pileBuf, MAX_PILE_SIZE);
			memcpy(&pileBufFlags[0], sucp.pileBufFlags, MAX_PILE_SIZE);
			client_free_packet(client_p, p_data);
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
		if (pileFaceupCount < MAX_PILE_SIZE)
		{
			pileFaceupCount++;
			int index = pileFacedownIndices[rnd_next_range(rnd, 0, MAX_PILE_SIZE - pileFaceupCount)];
			// need to choose a random tile to flip from only the ones that are faceup already
			//while (pileBufFlags[index] != pileTileState::facedown) { index = rnd_next_range(rnd, 0, MAX_PILE_SIZE - 1); }
			RemoveFacedownIndex(index);
			pileBufFlags[index] = pileTileState::faceup;
			flip_timer = 0;
			sortPile();
		}
		else
			printf("\nGG! Game is done!\n");
	}
#if RENDERING_CODE == true
	render_pile();
	render_player_words();
	batch_flush(batch_p);
#endif // SERVER_DEBUG_RENDERING == true

	uint64_t unix_time = unix_timestamp();
	server_update(server, dt, unix_time);

	static float send_update_pkt_timer = 0;
	send_update_pkt_timer += dt;
	if (send_update_pkt_timer > 1)
	{
#if DEBUG_PRINTS_NET == true
		printf("Sending board to client!\n");
#endif
		server_update_clients_packet sucp;
		memcpy(sucp.pileBuf, &pileBuf[0], MAX_PILE_SIZE);
		memcpy(sucp.pileBufFlags, &pileBufFlags[0], MAX_PILE_SIZE);
		server_send_to_all_clients(server, &sucp, sizeof(sucp), true);
		send_update_pkt_timer = 0;
	}
	server_event_t e;
	while (server_pop_event(server, &e)) {
		if (e.type == SERVER_EVENT_TYPE_NEW_CONNECTION) {
			printf("New connection from id %d, on index %d.\n", (int)e.u.new_connection.client_id, e.u.new_connection.client_index);
		}
		else if (e.type == SERVER_EVENT_TYPE_PAYLOAD_PACKET) {
			int c_idx = (int)e.u.payload_packet.client_index;
			const char* msg = (const char*)e.u.payload_packet.data;
#if DEBUG_PRINTS_NET == true
			printf("Got a message from client on index %d, \"%s\"\n", c_idx, msg);
#endif
			const char* msg_header = "kp:enter:";
			const int msg_header_len = strlen(msg_header);
			if (strcmp(msg, msg_header))
			{
				const char* word = msg + msg_header_len; // skip the header
				printf("A player is trying to make word: %s\n", word);
				if (is_a_word(word))
				{
					if (!wordAlreadyMade(&letterBuf[0]))
					{
						if (canPileSteal(word))
						{
							printf("can pile steal!\n");
							doPileSteal(c_idx, word);
						}
						else
						{
							tryAnagramSteal(word, c_idx);
							printf("cannot pile steal...\n");
						}
					}
				}
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
		if (is_a_word(&letterBuf[0]))
		{
			printf("%s is a word!\n", &letterBuf[0]);
			if (!wordAlreadyMade(&letterBuf[0]))
			{
				if (canPileSteal(&letterBuf[0]))
				{
					printf("can pile steal!\n");
					doPileSteal(0, &letterBuf[0]);
				}
				else
				{
					tryAnagramSteal(&letterBuf[0], 0);
					printf("cannot pile steal...\n");
				}
			}
		}
		else
			printf("%s is not a word...\n", &letterBuf[0]);
	}
}
void render_typing()
{
	update_letterBuf();
	float letterscale = 0.7;
	int tilewidth = letter_sprites[0].w * letterscale;
	v2 pos(- (float)strlen(&letterBuf[0]) / 2.0 * (float)tilewidth,
		PILE_DIM * tilewidth - 170);
	render_string(&letterBuf[0], tilewidth, pos, letterscale);
}
void main_loop()
{
	while (app_is_running())
	{
		float dt = calc_dt();
		app_update(dt);

		render_typing();
#if RENDERING_CODE == true
		batch_flush(batch_p);
#endif

#ifdef CLIENT
		client_update_code(dt);
#endif

#ifdef SERVER
		server_update_code(dt);
#endif
#if RENDERING_CODE == true
		app_present();
#endif
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
	printf("Setting up Client...\n");
	client_p = client_create(0, appID);
	
	// Must be unique for each different player in your game.
	uint64_t client_id = (uint64_t)rnd_next_range(rnd, 0, 9999999);
	printf("my client ID is: %d\n", (int)client_id);

	char server_address_and_port[100] = CLIENT_IP;
	strcat(server_address_and_port, ":");
	strcat(server_address_and_port, PORT);
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
	printf("Setting up Server...\n");
	char address_and_port[100] = SERVER_IP;
	strcat(address_and_port, ":");
	strcat(address_and_port, PORT);

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
	//const char* filedata;
	//char filepath[100] = "wordlists/";
	//char onedigit[2];
	//onedigit[0] = (n < 10) ? ('0'+n) : ('0');
	//onedigit[1] = '\0';
	//char twodigits[3];
	//twodigits[0] = (n < 10) ? ('0') : ('0'+(n/10));
	//twodigits[1] = (n < 10) ? ('0') : ('0'+(n%10));
	//twodigits[2] = '\0';
	////printf("\nonedigit:  %s", onedigit);
	////printf("\ntwodigits: %s", twodigits);

	////printf("\nfilepath: %s", filepath);
	//strcat(filepath, (n < 10) ? onedigit : twodigits);
	////printf("\nfilepath: %s", filepath);
	//strcat(filepath, "sort.txt");
	//
	//printf("\nfilepath: %s", filepath);

	//size_t filesize = 0;
	//file_system_read_entire_file_to_memory_and_nul_terminate(
	//	filepath,
	//	(void**)&filedata,
	//	&filesize
	//	);
	//
	//printf("\nfilesize: %d", (int)filesize);

	//const char* cur = filedata;
	//const char* end = filedata + filesize;
	//while(cur < end)
	//{
	//	array<string_t>* arr = nullptr;
	//	
	//	string_t tempkey = string_t(cur, cur + n);
	//	cur += n;
	//	printf("\n1tempkey:%s\n", tempkey.c_str());
	//	tempkey.incref();

	//	cur++; // comma skip

	//	string_t tempvalue = string_t(cur, cur + n);
	//	cur += n + 2;
	//	printf("\n2tempvalue:%s", tempvalue.c_str());
	//	tempvalue.incref();

	//	arr = fastDict.find(tempkey);
	//	if(!arr)
	//	{
	//		int temp = fastDict.count();
	//		printf("\ninserting key #%d", temp);
	//		fastDict.insert(tempkey, {tempvalue});
	//	}
	//	else
	//	{
	//		printf("\nadding...");
	//		arr->add(tempvalue);
	//	}
	//}

	//printf("\nfastDict count: %d\n",fastDict.count());
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

#if RENDERING_CODE == true
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
#endif
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
	for (int i = 0; i < MAX_WORD_LEN+1; i++)
	{
		letterBuf.push_back('\0');
	}

	// init pile memory
	for (int i = 0; i < MAX_PILE_SIZE; i++)
	{
		pileBuf.push_back('\0');
		pileSorted.push_back('\0');
		pileBufFlags.push_back(pileTileState::facedown);
		pileFacedownIndices[i] = i;
	}
	const char pnames[MAX_PLAYERS][MAX_WORD_LEN] =
	{
		{ "PLAYERONE" },
		{ "PLAYERTWO" },
		{ "PLAYERTHREE" },
		{ "PLAYERFOUR" },
		{ "PLAYERFIVE" },
		{ "PLAYERSIX" },
		{ "PLAYERSEVEN" },
		{ "PLAYEREIGHT" }
	};
	const char numtestwords = 4;
	const char testwords[MAX_WORDS_PER_PLAYER][MAX_WORD_LEN] =
	{
		{ "DOG" },
		{ "FATE" },
		{ "TEAM" },
		{ "HEAT" },
		{ "LSKDFJAKSDF" },
		{ "ASDJFSAFJASDFH" },
		{ "ASDJFSAFJASDF" },
		{ "ASDJFSAFJASD" },
		{ "ASDJFSAFJAS" },
		{ "ASDJFSAFJA" }
	};
	
	playerNames.resize(MAX_PLAYERS);
	// init player words memory
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		playerWords.push_back(std::vector<std::string>());
		for (int j = 0; j < MAX_WORDS_PER_PLAYER; j++)
		{
			playerWords[i].push_back(std::string());
			playerNames[i].resize(MAX_WORD_LEN,'\0');
			for (int k = 0; k < MAX_WORD_LEN + 1; k++)
			{
				playerWords[i][j].push_back('\0');
				playerNames[i][k] = '\0';
			}
		}

		for (int k = 0; k < MAX_WORD_LEN; k++)
			playerNames[i][k] = pnames[i][k];

		playerNumWords.push_back(0);
	}
	// init words made memory
	for (int i = 0; i < MAX_WORDS_MADE_HISTORY; i++)
	{
		wordsMade.push_back(std::string());
		for (int j = 0; j < MAX_WORD_LEN + 1; j++)
		{
			wordsMade[i].push_back('\0');
		}
	}
	//wordsMade[i][j] = '\0';

	numWordsMade = 0;
	// init test data memory
	if (TEST_DATA)
	{
		// TEST
		for (int i = 1; i < 2; i++)
		{
			for (int j = 0; j < min(MAX_WORDS_PER_PLAYER, numtestwords); j++)
			{
				for (int k = 0; k < MAX_WORD_LEN; k++)
					playerWords[i][j][k] = testwords[j][k];

				playerNumWords[i]++;
				wordWasMade(testwords[j]);
			}

		}
	}
	// common letter distribution for popular word games
	//A - 9, B - 2, C - 2, D - 4, E - 12, F - 2, G - 3, H - 2, I - 9, J - 1, K - 1,
	//L - 4, M - 2, N - 6, O - 8, P - 2, Q - 1, R - 6, S - 4, T - 6, U - 4, V - 2,
	//W - 2, X - 1, Y - 2, Z - 1
	char letter_distr[100] = "AAAAAAAAABBCCDDDDEEEEEEEEEEEEFFGGGHHIIIIIIIIIJKLLLLMMNNNNNNNOOOOOOOOPPQRRRRRRSSSSTTTTTTUUUUVVWWXYYZ";
	Shuffle(letter_distr);
	printf("\nDistribution: ");
	int len = strlen(letter_distr);
	for (int i = 0; i < len; i++)
	{
		pileBuf[i] = letter_distr[i];
		printf("%c", pileBuf[i]);
	}
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
void selectionSort(char arr[], int n)
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
	}
}
void sortPile()
{
	for (int i = 0; i < MAX_PILE_SIZE; i++)
	{
		if (pileBufFlags[i] == pileTileState::faceup)
			pileSorted[i] = pileBuf[i];
		else
			pileSorted[i] = '0';
	}
	selectionSort(&pileSorted[0], MAX_PILE_SIZE);
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
void doPileSteal(int playerID, const char* word)
{
	if (playerNumWords[playerID] >= MAX_WORDS_PER_PLAYER)
	{
		printf("This player already has the maximum number of words!\n");
		return;
	}

	int wordlen = strlen(word);
	for (int i = 0; i < wordlen; ++i)
	{
		int index = findFirstFaceupChar(word[i]);
		if (index >= 0)
		{
			playerWords[playerID][playerNumWords[playerID]][i] = word[i];
			pileBufFlags[index] = pileTileState::empty;
		}
		else
			printf("Error, char not found in pile\n");
	}
	playerNumWords[playerID]++;
	wordWasMade(word);
	printf("\n");
	for (int i = 0; i < wordlen; ++i) deleteLastTypedChar(); // for server testing
	pileFaceupCount -= wordlen;
	sortPile();
}
void tryAnagramSteal(const char* word, int playerID)
{
	int wordlen = strlen(word);
	for (int i = 0; i < MAX_PLAYERS; ++i) // player
	{
		for (int j = 0; j < playerNumWords[i]; ++j) // word
		{
			if (strlen(&playerWords[i][j][0]) == wordlen)
			{
				char sortedPword[MAX_WORD_LEN + 1];
				char sortedStr[MAX_WORD_LEN + 1];
				memset(sortedPword, '\0', MAX_WORD_LEN + 1);
				memset(sortedStr, '\0', MAX_WORD_LEN + 1);
				memcpy(sortedPword, &playerWords[i][j][0], wordlen);
				memcpy(sortedStr, word, wordlen);
				selectionSort(sortedPword, wordlen);
				selectionSort(sortedStr, wordlen);
				if (strcmp(sortedPword, sortedStr) == 0) // if can steal
				{
					if (playerNumWords[playerID] >= MAX_WORDS_PER_PLAYER)
					{
						printf("This player already has the maximum number of words!\n");
						return;
					}

					// give playerID the word
					for (int k = 0; k < wordlen; ++k)
						playerWords[playerID][playerNumWords[playerID]][k] = word[k];

					playerNumWords[playerID]++;

					// shift words up for player who lost a word
					for (int k = j; k < playerNumWords[i]-1; ++k) // each word after current
					{
						memset(&playerWords[i][k][0], '\0', MAX_WORD_LEN);
						memcpy(&playerWords[i][k][0], &playerWords[i][k+1][0], MAX_WORD_LEN);
					}
					memset(&playerWords[i][playerNumWords[i] - 1][0], '\0', MAX_WORD_LEN);

					playerNumWords[i]--;
					wordWasMade(word);

					for (int k = 0; k < wordlen; ++k) deleteLastTypedChar(); // clear typed letters
					printf("player %d anagram stole a word from player %d\n", playerID, i);
					return;
				}
			}
		}
	}
	printf("No anagram steals.\n");
}
//void remainingString(const char* word, const char* letters, char result[MAX_WORD_LEN])
//{
//	const char* empty = "";
//	char wordbuf[MAX_WORD_LEN];
//	int wordlen = strlen(word);
//	memcpy(wordbuf, word, wordlen);
//	while (wordlen > 0)
//	{
//		//if (!letters.Contains(word[word.Length - 1]))
//		if (!strstr(word, letters))
//		{
//			result = empty;
//			return;
//		}
//
//		int index = letters.IndexOf(word[word.Length - 1]);
//		word = word.Remove(word.Length - 1, 1);
//		letters = letters.Remove(index, 1);
//
//		wordlen = strlen(word);
//	}
//	return letters; // return the remainder
//}
void tryBuildSteal(const char* word, int playerID)
{
	// first check all player words to see if any are smaller than this one


	// of those that are smaller, see if they are contained in word

	// of those that are contained, get the remaining letters needed

	// check of those letters are part of the pile

	// if they are, steal the word

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
	
	return true;
}
int main(int argc, const char** argv)
{
#ifdef SERVER
	uint32_t app_options = CUTE_APP_OPTIONS_HIDDEN;
#endif
#ifdef CLIENT
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
#endif
	if (RENDERING_CODE)
	{
		app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	}
	app_make("Steal Words Multiplayer", 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, app_options, argv[0]);

#if RENDERING_CODE == true
	batch_p = sprite_get_batch();
	batch_set_projection(batch_p, matrix_ortho_2d(WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0));
#endif
	mount_content_folder();
	rnd = rnd_seed((uint64_t)time(0));

#if RENDERING_CODE == true
	app_init_imgui();
#endif
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
