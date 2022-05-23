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

/*************************************************************************************************/
// Function declarations
/*************************************************************************************************/
error_t make_test_connect_token(uint64_t unique_client_id, const char* address_and_port, uint8_t* connect_token_buffer);
void client_update_code(float dt);
void server_update_code(float dt);
void main_loop();
void panic(error_t err);
uint64_t unix_timestamp();
void mount_content_folder();
void client_init_code();
void server_init_code();
int main(int argc, const char** argv);

/*************************************************************************************************/
// Global data
/*************************************************************************************************/
uint16_t port = 5001;
uint64_t appID = 234;

batch_t* batch_p;
sprite_t letter_sprites[26];

rnd_t rnd;

list_t strlist;

dictionary<string_t, array<string_t>> fastDict;

//#define CLIENT
//#define SERVER

//#ifdef CLIENT
client_t* client_p;
uint32_t client_id;
const int letterBufSize = 15;
char letterBuf[letterBufSize+1];
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
// Function definitions
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
void client_check_input()
{
	for(int i='a';i<='z';++i)
	{
		if (key_was_pressed(key_button_t(i))) {
			if(strlen(letterBuf) < letterBufSize)
			{
				char letter[2];
				letter[0] = i;
				letter[1] = '\0';
				strcat(letterBuf, letter);
			}
		}	
	}
	if (key_was_pressed(KEY_BACKSPACE))
	{
		if(strlen(letterBuf))
		{
			letterBuf[strlen(letterBuf)-1] = '\0';
		}
	}
}
void client_update_code(float dt)
{	
	float offset = -7 * 64;
	for(int i=0;i<strlen(letterBuf);i++)
	{
		int letter_index = letterBuf[i]-'a';
		letter_sprites[letter_index].transform.p.x = offset;
		letter_sprites[letter_index].transform.p.y = -100;
		offset += 64;
		letter_sprites[letter_index].draw(batch_p);
	}

	batch_flush(batch_p);

	uint64_t unix_time = unix_timestamp();

	client_update(client_p, dt, unix_time);

	client_check_input();

	if (client_state_get(client_p) == CLIENT_STATE_CONNECTED) {
		static bool notify = false;
		if (!notify) {
			notify = true;
			printf("Connected! Press ESC to gracefully disconnect.\n");
		}

		static float t = 0;
		t += dt;
		if (t > 2) {
			const char* data = "What's up over there, Mr. Server?";
			int size = (int)strlen(data) + 1;
			client_send(client_p, data, size, false);
			t = 0;
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
	uint64_t unix_time = unix_timestamp();

	server_update(server, dt, unix_time);

	server_event_t e;
	while (server_pop_event(server, &e)) {
		if (e.type == SERVER_EVENT_TYPE_NEW_CONNECTION) {
			printf("New connection from id %d, on index %d.\n", (int)e.u.new_connection.client_id, e.u.new_connection.client_index);
		}
		else if (e.type == SERVER_EVENT_TYPE_PAYLOAD_PACKET) {
			printf("Got a message from client on index %d, \"%s\"\n", e.u.payload_packet.client_index, (const char*)e.u.payload_packet.data);
			server_free_packet(server, e.u.payload_packet.client_index, e.u.payload_packet.data);
		}
		else if (e.type == SERVER_EVENT_TYPE_DISCONNECTED) {
			printf("Client disconnected on index %d.\n", e.u.disconnected.client_index);
		}
	}
}
void main_loop()
{
	while (app_is_running())
	{
		float dt = calc_dt();
		app_update(dt);

#ifdef CLIENT
		client_update_code(dt);
#endif

		app_present();

#ifdef SERVER
		server_update_code(dt);
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
	printf("Setting up Client");
	client_p = client_create(0, appID);
	
	// Must be unique for each different player in your game.
	rnd = rnd_seed((uint64_t)time(0));
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
int main(int argc, const char** argv)
{
	uint32_t app_options = CUTE_APP_OPTIONS_DEFAULT_GFX_CONTEXT | CUTE_APP_OPTIONS_WINDOW_POS_CENTERED;
	app_make("Steal Words Multiplayer", 0, 0, 1024, 768, app_options, argv[0]);
	batch_p = sprite_get_batch();
	batch_set_projection(batch_p, matrix_ortho_2d(1024, 768, 0, 0));
	mount_content_folder();

#ifdef SERVER
	server_init_code();
#endif

#ifdef CLIENT
	client_init_code();
#endif

	app_init_imgui();
	
	const char* filedata;
	size_t filesize = 0;
	file_system_read_entire_file_to_memory_and_nul_terminate(
		"wordlists/2sort.txt",
		(void**)&filedata,
		&filesize
		);
	
	printf("filesize: %d", (int)filesize);

	const char* cur = filedata;
	const char* end = filedata + filesize;
	while(cur < end)
	{
		array<string_t>* arr = nullptr;
		
		string_t tempkey = string_t(cur, cur + 2);
		cur += 2;
		//printf("1tempkey:%s\n", tempkey.c_str());
		tempkey.incref();

		cur++; // comma skip

		string_t tempvalue = string_t(cur, cur + 4);
		cur += 4;
		//printf("2tempvalue:%s\n", tempvalue.c_str());
		tempvalue.incref();

		//printf("find...");
		arr = fastDict.find(tempkey);
		if(!arr)
		{
			//printf("inserting...");
			fastDict.insert(tempkey, {tempvalue});
		}
		else
		{
			//printf("adding...");
			arr->add(tempvalue);
		}
	}

	printf("fastDict count: %d\n",fastDict.count());
	//printf("key: %s", fastDict.keys()[0].c_str());
	
	for(int i=0;i<fastDict.count();i++)
	{
		printf("key: %s", fastDict.keys()[i].c_str());
		for(int j=0;j<fastDict.items()[i].count();j++)
		{
			printf(", value: %s", fastDict.items()[i][j].c_str());
		}
		printf("\n");
	}

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
	}



	main_loop();

	app_destroy();

	return 0;
}
