/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2015 Mino <mino@minomino.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Mino: Most of this is from Q3 sources, but obviously the structs aren't
 * exactly the same, so there's a good number of modifications to make it
 * fit QL. The end of the file has a bunch of stuff I added. Might want
 * to refactor it. TODO.
*/

#ifndef QUAKE_COMMON_H
#define QUAKE_COMMON_H

#include <stdint.h>

#include "patterns.h"
#include "common.h"

#define	CS_SCORES1				6
#define	CS_SCORES2				7
#define CS_VOTE_TIME			8
#define CS_VOTE_STRING			9
#define	CS_VOTE_YES				10
#define	CS_VOTE_NO				11

#define MAX_CLIENTS 64
#define MAX_CHALLENGES  1024
#define MAX_MSGLEN  16384
#define MAX_PS_EVENTS   2
#define MAX_MAP_AREA_BYTES  32  // bit vector of area visibility
#define MAX_INFO_STRING 1024
#define MAX_RELIABLE_COMMANDS   64  // max string commands buffered for restransmit
#define MAX_STRING_CHARS    1024    // max length of a string passed to Cmd_TokenizeString
#define MAX_NAME_LENGTH 32  // max length of a client name
#define MAX_QPATH   64  // max length of a quake game pathname
#define MAX_DOWNLOAD_WINDOW 8   // max of eight download frames
#define MAX_NETNAME			36
#define PACKET_BACKUP   32  // number of old messages that must be kept on client and
                            // server for delta comrpession and ping estimation
#define	PACKET_MASK	(PACKET_BACKUP-1)
#define MAX_ENT_CLUSTERS    16
#define MAX_MODELS  256 // these are sent over the net as 8 bits
#define MAX_CONFIGSTRINGS   1024
#define GENTITYNUM_BITS     10      // don't need to send any more
#define MAX_GENTITIES       (1<<GENTITYNUM_BITS)
#define MAX_ITEM_MODELS 4
#define MAX_SPAWN_VARS 64
#define MAX_SPAWN_VARS_CHARS 4096
#define BODY_QUEUE_SIZE 8

// bit field limits
#define MAX_STATS               16
#define MAX_PERSISTANT          16
#define MAX_POWERUPS            16
#define MAX_WEAPONS             16

// Button flags
#define	BUTTON_ATTACK		1
#define	BUTTON_TALK			2			// displays talk balloon and disables actions
#define	BUTTON_USE_HOLDABLE	4			// Mino: +button2
#define	BUTTON_GESTURE		8			// Mino: +button3
#define	BUTTON_WALKING		16
// Block of unused button flags, or at least flags I couldn't trigger.
// Q3 used them for bot commands, so probably unused in QL.
#define BUTTON_UNUSED1		32
#define	BUTTON_UNUSED2		64
#define BUTTON_UNUSED3		128
#define BUTTON_UNUSED4		256
#define BUTTON_UNUSED5		512
#define BUTTON_UNUSED6		1024
#define	BUTTON_UPMOVE		2048  // Mino: Not in Q3. I'm guessing it's for cg_autohop.
#define	BUTTON_ANY			4096  // any key whatsoever
#define BUTTON_IS_ACTIVE 	65536 // Mino: No idea what it is, but it goes off after a while of being
								  //       AFK, then goes on after being active for a while.

// eflags
#define	EF_DEAD				0x00000001		// don't draw a foe marker over players with EF_DEAD
#define EF_TICKING			0x00000002		// used to make players play the prox mine ticking sound
#define	EF_TELEPORT_BIT		0x00000004		// toggled every time the origin abruptly changes
#define	EF_AWARD_EXCELLENT	0x00000008		// draw an excellent sprite
#define EF_PLAYER_EVENT		0x00000010
#define	EF_BOUNCE			0x00000010		// for missiles
#define	EF_BOUNCE_HALF		0x00000020		// for missiles
#define	EF_AWARD_GAUNTLET	0x00000040		// draw a gauntlet sprite
#define	EF_NODRAW			0x00000080		// may have an event, but no model (unspawned items)
#define	EF_FIRING			0x00000100		// for lightning gun
#define	EF_KAMIKAZE			0x00000200
#define	EF_MOVER_STOP		0x00000400		// will push otherwise
#define EF_AWARD_CAP		0x00000800		// draw the capture sprite
#define	EF_TALK				0x00001000		// draw a talk balloon
#define	EF_CONNECTION		0x00002000		// draw a connection trouble sprite
#define	EF_VOTED			0x00004000		// already cast a vote
#define	EF_AWARD_IMPRESSIVE	0x00008000		// draw an impressive sprite
#define	EF_AWARD_DEFEND		0x00010000		// draw a defend sprite
#define	EF_AWARD_ASSIST		0x00020000		// draw a assist sprite
#define EF_AWARD_DENIED		0x00040000		// denied
#define EF_TEAMVOTED		0x00080000		// already cast a team vote

typedef enum {qfalse, qtrue} qboolean;
typedef unsigned char byte;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef int fileHandle_t;

// The permission levels used by QL's admin commands.
typedef enum {
	PRIV_NONE,
	PRIV_MOD,
	PRIV_ADMIN
} privileges_t;

// Vote type. As opposed to in Q3, votes are counted every frame.
typedef enum {
	VOTE_NONE,
	VOTE_NO,
	VOTE_YES,
	VOTE_ABSTAIN,
	VOTE_FORCE_PASS,
	VOTE_FORCE_FAIL
} voteType_t;

typedef enum {
    CS_FREE,        // can be reused for a new connection
    CS_ZOMBIE,      // client has been disconnected, but don't reuse
                    // connection for a couple seconds
    CS_CONNECTED,   // has been assigned to a client_t, but no gamestate yet
    CS_PRIMED,      // gamestate has been sent, but client hasn't sent a usercmd
    CS_ACTIVE       // client is fully in game
} clientState_t;

typedef enum {
	GAME_INIT,	// ( int levelTime, int randomSeed, int restart );
	// init and shutdown will be called every single level
	// The game should call G_GET_ENTITY_TOKEN to parse through all the
	// entity configuration text and spawn gentities.

	GAME_SHUTDOWN,	// (void);

	GAME_CLIENT_CONNECT,	// ( int clientNum, qboolean firstTime, qboolean isBot );
	// return NULL if the client is allowed to connect, otherwise return
	// a text string with the reason for denial

	GAME_CLIENT_BEGIN,				// ( int clientNum );

	GAME_CLIENT_USERINFO_CHANGED,	// ( int clientNum );

	GAME_CLIENT_DISCONNECT,			// ( int clientNum );

	GAME_CLIENT_COMMAND,			// ( int clientNum );

	GAME_CLIENT_THINK,				// ( int clientNum );

	GAME_RUN_FRAME,					// ( int levelTime );

	GAME_CONSOLE_COMMAND,			// ( void );
	// ConsoleCommand will be called when a command has been issued
	// that is not recognized as a builtin function.
	// The game can issue trap_argc() / trap_argv() commands to get the command
	// and parameters.  Return qfalse if the game doesn't recognize it as a command.

	BOTAI_START_FRAME				// ( int time );
} gameExport_t;

typedef enum {
	_EV_NONE,

	EV_FOOTSTEP,
	EV_FOOTSTEP_METAL,
	EV_FOOTSPLASH,
	EV_FOOTWADE,
	EV_SWIM,

	EV_FALL_SHORT,
	EV_FALL_MEDIUM,
	EV_FALL_FAR,

	EV_JUMP_PAD,			// boing sound at origin, jump sound on player

	EV_JUMP,
	EV_WATER_TOUCH,	// foot touches
	EV_WATER_LEAVE,	// foot leaves
	EV_WATER_UNDER,	// head touches
	EV_WATER_CLEAR,	// head leaves

	EV_ITEM_PICKUP,			// normal item pickups are predictable
	EV_GLOBAL_ITEM_PICKUP,	// powerup / team sounds are broadcast to everyone

	EV_NOAMMO,
	EV_CHANGE_WEAPON,
    EV_UNKNOWN1, // Mino: Position is correct here. Makes no sound with param 1.
	EV_FIRE_WEAPON,

	EV_USE_ITEM0,
	EV_USE_ITEM1,
	EV_USE_ITEM2,
	EV_USE_ITEM3,
	EV_USE_ITEM4,
	EV_USE_ITEM5,
	EV_USE_ITEM6,
	EV_USE_ITEM7,
	EV_USE_ITEM8,
	EV_USE_ITEM9,
	EV_USE_ITEM10,
	EV_USE_ITEM11,
	EV_USE_ITEM12,
	EV_USE_ITEM13,
	EV_USE_ITEM14,
	EV_USE_ITEM15,

	EV_ITEM_RESPAWN,
	EV_ITEM_POP,
	EV_PLAYER_TELEPORT_IN,
	EV_PLAYER_TELEPORT_OUT,

	EV_GRENADE_BOUNCE,		// eventParm will be the soundindex

	EV_GENERAL_SOUND,
	EV_GLOBAL_SOUND,		// no attenuation
	EV_GLOBAL_TEAM_SOUND,

	EV_BULLET_HIT_FLESH,
	EV_BULLET_HIT_WALL,

	EV_MISSILE_HIT,
	EV_MISSILE_MISS,
	EV_MISSILE_MISS_METAL,
	EV_RAILTRAIL,
	EV_SHOTGUN,
	EV_BULLET,				// otherEntity is the shooter

	EV_PAIN,
	EV_DEATH1,
	EV_DEATH2,
	EV_DEATH3,
    EV_DROWN,
	//EV_OBITUARY,
    
    EV_UNKNOWN2, // Mino: Position is correct. Makes no sound with param 0.

	EV_POWERUP_QUAD,
	EV_POWERUP_BATTLESUIT,
	EV_POWERUP_REGEN,

    EV_AMMO_PICKUP,
	EV_GIB_PLAYER,			// gib a previously living player
	EV_SCOREPLUM,			// score plum

//#ifdef MISSIONPACK
	EV_PROXIMITY_MINE_STICK,
	EV_PROXIMITY_MINE_TRIGGER,
	EV_KAMIKAZE,			// kamikaze explodes
	EV_OBELISKEXPLODE,		// obelisk explodes
	EV_OBELISKPAIN,			// obelisk is in pain
	EV_INVUL_IMPACT,		// invulnerability sphere impact
	EV_JUICED,				// invulnerability juiced effect
	EV_LIGHTNINGBOLT,		// lightning bolt bounced of invulnerability sphere
//#endif

	EV_DEBUG_LINE,
	EV_STOPLOOPINGSOUND,
	EV_TAUNT,
	EV_TAUNT_YES,
	EV_TAUNT_NO,
	EV_TAUNT_FOLLOWME,
	EV_TAUNT_GETFLAG,
	EV_TAUNT_GUARDBASE,
	EV_TAUNT_PATROL,
    EV_UNKNOWN3,
    EV_UNKNOWN4,
    EV_UNKNOWN5, // Mino: Makes the client (clients?) crash.

} entity_event_t;

typedef enum {
	IT_BAD,
	IT_WEAPON,				// EFX: rotate + upscale + minlight
	IT_AMMO,				// EFX: rotate
	IT_ARMOR,				// EFX: rotate + minlight
	IT_HEALTH,				// EFX: static external sphere + rotating internal
	IT_POWERUP,				// instant on, timer based
							// EFX: rotate + external ring that rotates
	IT_HOLDABLE,			// single use, holdable item
							// EFX: rotate + bob
	IT_PERSISTANT_POWERUP,
	IT_TEAM
} itemType_t;

typedef enum {
	WP_NONE,

	WP_GAUNTLET,
	WP_MACHINEGUN,
	WP_SHOTGUN,
	WP_GRENADE_LAUNCHER,
	WP_ROCKET_LAUNCHER,
	WP_LIGHTNING,
	WP_RAILGUN,
	WP_PLASMAGUN,
	WP_BFG,
	WP_GRAPPLING_HOOK,
//#ifdef MISSIONPACK
	WP_NAILGUN,
	WP_PROX_LAUNCHER,
	WP_CHAINGUN,
//#endif

	WP_NUM_WEAPONS
} weapon_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} playerTeamStateState_t;

typedef enum {
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_SPECTATOR,

	TEAM_NUM_TEAMS
} team_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

// movers are things like doors, plats, buttons, etc
typedef enum {
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1
} moverState_t;

enum cvar_flags {
  CVAR_ARCHIVE = 1,
  CVAR_USERINFO = 2,
  CVAR_SERVERINFO = 4,
  CVAR_SYSTEMINFO = 8,
  CVAR_INIT = 16,
  CVAR_LATCH = 32,
  CVAR_ROM = 64,
  CVAR_USER_CREATED = 128,
  CVAR_TEMP = 256,
  CVAR_CHEAT = 512,
  CVAR_NORESTART = 1024,
  CVAR_UNKOWN1 = 2048,
  CVAR_UNKOWN2 = 4096,
  CVAR_UNKOWN3 = 8192,
  CVAR_UNKOWN4 = 16384,
  CVAR_UNKOWN5 = 32768,
  CVAR_UNKOWN6 = 65536,
  CVAR_UNKOWN7 = 131072,
  CVAR_UNKOWN8 = 262144,
  CVAR_UNKOWN9 = 524288,
  CVAR_UNKOWN10 = 1048576
};

// paramters for command buffer stuffing
typedef enum {
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
						// because some commands might cause the VM to be unloaded...
	EXEC_INSERT,		// insert at current position, but don't run yet
	EXEC_APPEND			// add to end of the command buffer (normal case)
} cbufExec_t;

// Mino: Quite different from Q3. Not sure on everything.
typedef struct cvar_s {
	char		*name;
	char		*string;
	char		*resetString;		// cvar_restart will reset to this value
	char		*latchedString;		// for CVAR_LATCH vars
    char        *defaultString;
    char		*minimumString;
    char		*maximumString;
	int			flags;
	qboolean	modified;
    uint8_t     _unknown2[4];
	int			modificationCount;	// incremented each time the cvar is changed
	float		value;				// atof( string )
	int			integer;			// atoi( string )
    uint8_t     _unknown3[8];
	struct cvar_s *next;
	struct cvar_s *hashNext;
} cvar_t;

typedef struct {
    qboolean    allowoverflow;  // if false, do a Com_Error
    qboolean    overflowed;     // set to true if the buffer size failed (with allowoverflow set)
    qboolean    oob;            // set to true if the buffer size failed (with allowoverflow set)
    byte    *data;
    int     maxsize;
    int     cursize;
    int     readcount;
    int     bit;                // for bitwise reads and writes
} msg_t;

typedef struct usercmd_s {
    int             serverTime;
    int             angles[3];
    int             buttons;
    byte            weapon;           // weapon 
    byte 			_unknown1 ,fov;
	signed char forwardmove, rightmove, upmove;
	byte 		_unknown2, _unknown3;
} usercmd_t;

typedef enum {
    NS_CLIENT,
    NS_SERVER
} netsrc_t;

typedef enum {
    NA_BOT,
    NA_BAD,                 // an address lookup failed
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IPX,
    NA_BROADCAST_IPX
} netadrtype_t;

typedef enum {
    TR_STATIONARY,
    TR_INTERPOLATE,             // non-parametric, but interpolate between snapshots
    TR_LINEAR,
    TR_LINEAR_STOP,
    TR_SINE,                    // value = base + sin( time / duration ) * delta
    TR_GRAVITY
} trType_t;

typedef struct {
    netadrtype_t    type;

    byte    ip[4];
    byte    ipx[10];

    unsigned short  port;
} netadr_t;

typedef struct {
    netsrc_t    sock;

    int         dropped;            // between last packet and previous

    netadr_t    remoteAddress;
    int         qport;              // qport value to write when transmitting

    // sequencing variables
    int         incomingSequence;
    int         outgoingSequence;

    // incoming fragment assembly buffer
    int         fragmentSequence;
    int         fragmentLength; 
    byte        fragmentBuffer[MAX_MSGLEN];

    // outgoing fragment buffer
    // we need to space out the sending of large fragmented messages
    qboolean    unsentFragments;
    int         unsentFragmentStart;
    int         unsentLength;
    byte        unsentBuffer[MAX_MSGLEN];
} netchan_t;

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
typedef struct playerState_s {
    int         commandTime;    // cmd->serverTime of last executed command
    int         pm_type;
    int         bobCycle;       // for view bobbing and footstep generation
    int         pm_flags;       // ducked, jump_held, etc
    int         pm_time;

    vec3_t      origin;
    vec3_t      velocity;
    int         weaponTime;
    int         gravity;
    int         speed;
    int         delta_angles[3];    // add to command angles to get view direction
                                    // changed by spawns, rotating objects, and teleporters

    int         groundEntityNum;// ENTITYNUM_NONE = in air

    int         legsTimer;      // don't change low priority animations until this runs out
    int         legsAnim;       // mask off ANIM_TOGGLEBIT

    int         torsoTimer;     // don't change low priority animations until this runs out
    int         torsoAnim;      // mask off ANIM_TOGGLEBIT

    int         movementDir;    // a number 0 to 7 that represents the reletive angle
                                // of movement to the view angle (axial and diagonals)
                                // when at rest, the value will remain unchanged
                                // used to twist the legs during strafing

    vec3_t      grapplePoint;   // location of grapple to pull towards if PMF_GRAPPLE_PULL

    int         eFlags;         // copied to entityState_t->eFlags

    int         eventSequence;  // pmove generated events
    int         events[MAX_PS_EVENTS];
    int         eventParms[MAX_PS_EVENTS];

    int         externalEvent;  // events set on player from another source
    int         externalEventParm;

    // Mino: I switched externalEventTime and clientNum so that clientNum alligns correctly,
    //		 but I doubt externalEventTime is what it's supposed to be.
    int         clientNum;      // ranges from 0 to MAX_CLIENTS-1
    int         externalEventTime;


    int         weapon;         // copied to entityState_t->weapon
    int         weaponstate;

    vec3_t      viewangles;     // for fixed views
    int         viewheight;

    // damage feedback
    int         damageEvent;    // when it changes, latch the other parms
    int         damageYaw;
    int         damagePitch;
    int         damageCount;

    int         stats[MAX_STATS];
    int         persistant[MAX_PERSISTANT]; // stats that aren't cleared on death
    int         powerups[MAX_POWERUPS]; // level.time that the powerup runs out
    int         ammo[MAX_WEAPONS];

    int         generic1;
    int         loopSound;
    int         jumppad_ent;    // jumppad entity hit this frame

    // not communicated over the net at all
    int         ping;           // server to game info for scoreboard
    int         pmove_framecount;   // FIXME: don't transmit over the network
    int         jumppad_frame;
    int         entityEventSequence;
} playerState_t;

typedef struct {
    int             areabytes;
    byte            areabits[MAX_MAP_AREA_BYTES];       // portalarea visibility bits
    playerState_t   ps;
    int             num_entities;
    int             first_entity;       // into the circular sv_packet_entities[]
                                        // the entities MUST be in increasing state number
                                        // order, otherwise the delta compression will fail
    int             messageSent;        // time the message was transmitted
    int             messageAcked;       // time the message was acked
    int             messageSize;        // used to rate drop packets
} clientSnapshot_t;

typedef struct netchan_buffer_s {
    msg_t           msg;
    byte            msgBuffer[MAX_MSGLEN];
    struct netchan_buffer_s *next;
} netchan_buffer_t;

typedef struct {
    trType_t    trType;
    int     trTime;
    int     trDuration;         // if non 0, trTime + trDuration = stop time
    vec3_t  trBase;
    vec3_t  trDelta;            // velocity, etc
} trajectory_t;

typedef struct entityState_s {
    int     number;         // entity index
    int     eType;          // entityType_t
    int     eFlags;

    trajectory_t    pos;    // for calculating position
    trajectory_t    apos;   // for calculating angles

    int     time;
    int     time2;

    vec3_t  origin;
    vec3_t  origin2;

    vec3_t  angles;
    vec3_t  angles2;
    
    // Mino:  Not sure where these go, but it'll line up clientNum correctly.
    int8_t  _unknown1[8];

    int     otherEntityNum; // shotgun sources, etc
    int     otherEntityNum2;

    int     groundEntityNum;    // -1 = in air

    int     constantLight;  // r + (g<<8) + (b<<16) + (intensity<<24)
    int     loopSound;      // constantly loop this sound

    int     modelindex;
    int     modelindex2;
    int     clientNum;      // 0 to (MAX_CLIENTS - 1), for players and corpses
    int     frame;

    int     solid;          // for client side prediction, trap_linkentity sets this properly

    int     event;          // impulse events -- muzzle flashes, footsteps, etc
    int     eventParm;

    // for players
    int     powerups;       // bit flags
    int     weapon;         // determines weapon and flash model, etc
    int     legsAnim;       // mask off ANIM_TOGGLEBIT
    int     torsoAnim;      // mask off ANIM_TOGGLEBIT

    int     generic1;
} entityState_t;

typedef struct {
    entityState_t   s;              // communicated by server to clients

    qboolean    linked;             // qfalse if not in any good cluster
    int         linkcount;

    int         svFlags;            // SVF_NOCLIENT, SVF_BROADCAST, etc

    // only send to this client when SVF_SINGLECLIENT is set    
    // if SVF_CLIENTMASK is set, use bitmask for clients to send to (maxclients must be <= 32, up to the mod to enforce this)
    int         singleClient;       

    qboolean    bmodel;             // if false, assume an explicit mins / maxs bounding box
                                    // only set by trap_SetBrushModel
    vec3_t      mins, maxs;
    int         contents;           // CONTENTS_TRIGGER, CONTENTS_SOLID, CONTENTS_BODY, etc
                                    // a non-solid entity should set to 0

    vec3_t      absmin, absmax;     // derived from mins/maxs and origin + rotation

    // currentOrigin will be used for all collision detection and world linking.
    // it will not necessarily be the same as the trajectory evaluation for the current
    // time, because each entity must be moved one at a time after time is advanced
    // to avoid simultanious collision issues
    vec3_t      currentOrigin;
    vec3_t      currentAngles;

    // when a trace call is made and passEntityNum != ENTITYNUM_NONE,
    // an ent will be excluded from testing if:
    // ent->s.number == passEntityNum   (don't interact with self)
    // ent->s.ownerNum = passEntityNum  (don't interact with your own missiles)
    // entity[ent->s.ownerNum].ownerNum = passEntityNum (don't interact with other missiles from owner)
    int         ownerNum;
} entityShared_t;

typedef struct {
    entityState_t   s;              // communicated by server to clients
    entityShared_t  r;              // shared by both the server system and game
} sharedEntity_t;

typedef struct client_s {
    clientState_t   state;
    char            userinfo[MAX_INFO_STRING];      // name, etc

    char            reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
    int             reliableSequence;       // last added reliable message, not necesarily sent or acknowledged yet
    int             reliableAcknowledge;    // last acknowledged reliable message
    int             reliableSent;           // last sent reliable message, not necesarily acknowledged yet
    int             messageAcknowledge;

    int             gamestateMessageNum;    // netchan->outgoingSequence of gamestate
    int             challenge;

    usercmd_t       lastUsercmd;
    int             lastMessageNum;     // for delta compression
    int             lastClientCommand;  // reliable client message sequence
    char            lastClientCommandString[MAX_STRING_CHARS];
    sharedEntity_t  *gentity;           // SV_GentityNum(clientnum)
    char            name[MAX_NAME_LENGTH];          // extracted from userinfo, high bits masked
    
    // Mino: I think everything above this is correct. Below is a mess.
    
    // downloading
    char            downloadName[MAX_QPATH]; // if not empty string, we are downloading
    fileHandle_t    download;           // file being downloaded
    int             downloadSize;       // total bytes (can't use EOF because of paks)
    int             downloadCount;      // bytes sent
    int             downloadClientBlock;    // last block we sent to the client, awaiting ack
    int             downloadCurrentBlock;   // current block number
    int             downloadXmitBlock;  // last block we xmited
    unsigned char   *downloadBlocks[MAX_DOWNLOAD_WINDOW];   // the buffers for the download blocks
    int             downloadBlockSize[MAX_DOWNLOAD_WINDOW];
    qboolean        downloadEOF;        // We have sent the EOF block
    int             downloadSendTime;   // time we last got an ack from the client

    int             deltaMessage;       // frame last client usercmd message
    int             nextReliableTime;   // svs.time when another reliable command will be allowed
    int             lastPacketTime;     // svs.time when packet was last received
    int             lastConnectTime;    // svs.time when connection started
    int             nextSnapshotTime;   // send another snapshot when svs.time >= nextSnapshotTime
    qboolean        rateDelayed;        // true if nextSnapshotTime was set based on rate instead of snapshotMsec
    int             timeoutCount;       // must timeout a few frames in a row so debugging doesn't break
    clientSnapshot_t    frames[PACKET_BACKUP];  // updates can be delta'd from here
    int             ping;
    int             rate;               // bytes / second
    int             snapshotMsec;       // requests a snapshot every snapshotMsec unless rate choked
    int             pureAuthentic;
    qboolean  gotCP; // TTimo - additional flag to distinguish between a bad pure checksum, and no cp command at all
    netchan_t       netchan;
    netchan_buffer_t *netchan_start_queue;
    netchan_buffer_t **netchan_end_queue;
    
    // Mino: Holy crap. A bunch of data was added. I have no idea where it actually goes,
    // but this will at least correct sizeof(client_t).
#if defined(__x86_64__) || defined(_M_X64)
    uint8_t         _unknown2[40776];
#elif defined(__i386) || defined(_M_IX86)
    uint8_t         _unknown2[40804]; // TODO: Outdated.
#endif

    // Mino: Woohoo! How nice of them to put the SteamID last.
    uint64_t        steam_id;
} client_t;

//
// SERVER
//

typedef struct {
    netadr_t    adr;
    int         challenge;
    int         time;               // time the last packet was sent to the autherize server
    int         pingTime;           // time the challenge response was sent to client
    int         firstTime;          // time the adr was first used, for authorize timeout checks
    qboolean    connected;
} challenge_t;

// this structure will be cleared only when the game dll changes
typedef struct {
    qboolean    initialized;                // sv_init has completed
    int         time;                       // will be strictly increasing across level changes
    int         snapFlagServerBit;          // ^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()
    client_t    *clients;                   // [sv_maxclients->integer];
    int         numSnapshotEntities;        // sv_maxclients->integer*PACKET_BACKUP*MAX_PACKET_ENTITIES
    int         nextSnapshotEntities;       // next snapshotEntities to use
    entityState_t   *snapshotEntities;      // [numSnapshotEntities]
    int         nextHeartbeatTime;
    challenge_t challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting
    netadr_t    redirectAddress;            // for rcon return messages
    netadr_t    authorizeAddress;           // for rcon return messages
} serverStatic_t;

typedef struct svEntity_s {
    struct worldSector_s *worldSector;
    struct svEntity_s *nextEntityInWorldSector;
    
    entityState_t   baseline;       // for delta compression of initial sighting
    int         numClusters;        // if -1, use headnode instead
    int         clusternums[MAX_ENT_CLUSTERS];
    int         lastCluster;        // if all the clusters don't fit in clusternums
    int         areanum, areanum2;
    int         snapshotCounter;    // used to prevent double adding from portal views
} svEntity_t;

typedef struct worldSector_s {
    int     axis;       // -1 = leaf node
    float   dist;
    struct worldSector_s    *children[2];
    svEntity_t  *entities;
} worldSector_t;

typedef enum {
    SS_DEAD,            // no map loaded
    SS_LOADING,         // spawning level entities
    SS_GAME             // actively running
} serverState_t;

typedef struct {
    serverState_t   state;
    qboolean        restarting;         // if true, send configstring changes during SS_LOADING
    int             serverId;           // changes each server start
    int             restartedServerId;  // serverId before a map_restart
    int             checksumFeed;       // the feed key that we use to compute the pure checksum strings
    // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
    // the serverId associated with the current checksumFeed (always <= serverId)
    int       checksumFeedServerId; 
    int             snapshotCounter;    // incremented for each snapshot built
    int             timeResidual;       // <= 1000 / sv_frame->value
    int             nextFrameTime;      // when time > nextFrameTime, process world
    struct cmodel_s *models[MAX_MODELS];
    char            *configstrings[MAX_CONFIGSTRINGS];
    svEntity_t      svEntities[MAX_GENTITIES];

    char            *entityParsePoint;  // used during game VM init

    // the game virtual machine will update these on init and changes
    sharedEntity_t  *gentities;
    int             gentitySize;
    int             num_entities;       // current number, <= MAX_GENTITIES

    playerState_t   *gameClients;
    int             gameClientSize;     // will be > sizeof(playerState_t) due to game private data

    int             restartTime;
} server_t;

typedef struct {
	playerTeamStateState_t	state;

	int			location;

	int			captures;
	int			basedefense;
	int			carrierdefense;
	int			flagrecovery;
	int			fragcarrier;
	int			assists;

	float		lasthurtcarrier;
	float		lastreturnedflag;
//	float		flagsince;
//	float		lastfraggedcarrier;
} playerTeamState_t;

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct {
	clientConnected_t	connected;	
	usercmd_t	cmd;				// we would lose angles if not persistant
	qboolean	localClient;		// true if "ip" info key is "localhost"
	qboolean	initialSpawn;		// the first spawn should be at a cool location
	qboolean	predictItemPickup;	// based on cg_predictItems userinfo
	//qboolean	pmoveFixed;			//
	char		netname[MAX_NETNAME];
	int			maxHealth;			// for handicapping
	int			enterTime;			// level.time the client entered the game
	playerTeamState_t teamState;	// status in teamplay games
	voteType_t voteType; // Mino: Stuff around here is iffy.
	int unknown;
	int			voteCount;			// to prevent people from constantly calling votes
	int			teamVoteCount;		// to prevent people from constantly calling votes
	qboolean	teamInfo;			// send team overlay updates?
} clientPersistant_t;

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct {
	team_t		sessionTeam;
	int			spectatorTime;		// for determining next-in-line to play
	spectatorState_t	spectatorState;
	int			spectatorClient;	// for chasecam and follow mode
	int			wins, losses;		// tournament stats
	qboolean	teamLeader;			// true when this client is a team leader
} clientSession_t;

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t	ps;				// communicated by server to clients

	uint8_t	unknown1[124];
	// the rest of the structure is private to game
	clientPersistant_t	pers;
	clientSession_t		sess;

	qboolean	readyToExit;		// wishes to leave the intermission

	qboolean	noclip;

	int			lastCmdTime;		// level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	int			buttons;
	int			oldbuttons;
	int			latched_buttons;

	vec3_t		oldOrigin;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int			damage_armor;		// damage absorbed by armor
	int			damage_blood;		// damage taken out of health
	int			damage_knockback;	// impact damage
	vec3_t		damage_from;		// origin for vector calculation
	qboolean	damage_fromWorld;	// if true, don't use the damage_from vector

	int			accurateCount;		// for "impressive" reward sound

	int			accuracy_shots;		// total number of shots
	int			accuracy_hits;		// total number of hits

	//
	int			lastkilled_client;	// last client that this client killed
	int			lasthurt_client;	// last client that damaged this client
	int			lasthurt_mod;		// type of damage the client did

	// timers
	int			respawnTime;		// can respawn when time > this, force after g_forcerespwan
	int			inactivityTime;		// kick players when time > this
	qboolean	inactivityWarning;	// qtrue if the five seoond warning has been given
	int			rewardTime;			// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	//int			airOutTime;
	privileges_t privileges; // Mino: Stuff above and under are probably completely wrong.

	int			lastKillTime;		// for multiple kill rewards

	qboolean	fireHeld;			// used for hook
	gentity_t	*hook;				// grapple hook if out

	int			switchTeamTime;		// time the player switched teams

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int			timeResidual;

//#ifdef MISSIONPACK
	gentity_t	*persistantPowerup;
	int			portalID;
	int			ammoTimes[WP_NUM_WEAPONS];
	int			invulnerabilityTime;
//#endif

	char		*areabits;
};

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;


// a trace is returned when a box is swept through the world
typedef struct {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact, transformed to world space
	int			surfaceFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			entityNum;	// entity the contacted sirface is a part of
} trace_t;

typedef struct gitem_s {
	char		*classname;	// spawning name
	char		*pickup_sound;
	char		*world_model[MAX_ITEM_MODELS];

	char		*icon;
	char		*pickup_name;	// for printing on pickup

	int			quantity;		// for ammo how much, or duration of powerup
	itemType_t  giType;			// IT_* flags

	int			giTag;

	char		*precaches;		// string of all models and images this item will use
	char		*sounds;		// string of all sounds this item will use
} gitem_t;

struct gentity_s {
	entityState_t	s;				// communicated by server to clients
	entityShared_t	r;				// shared by both the server system and game
    uint8_t _unknown1[40];

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	struct gclient_s	*client;			// NULL if not a client

	qboolean	inuse;

	char		*classname;			// set in QuakeEd
	int			spawnflags;			// set in QuakeEd

	qboolean	neverFree;			// if true, FreeEntity will only unlink
									// bodyque uses this

	int			flags;				// FL_* variables

	char		*model;
	char		*model2;
	int			freetime;			// level.time when the object was freed
	
	int			eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean	freeAfterEvent;
	qboolean	unlinkAfterEvent;

	qboolean	physicsObject;		// if true, it can be pushed by movers and fall off edges
									// all game items are physicsObjects, 
	float		physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int			clipmask;			// brushes with this content value will be collided against
									// when moving.  items and corpses do not collide against
									// players, for instance

	// movers
	moverState_t moverState;
	int			soundPos1;
	int			sound1to2;
	int			sound2to1;
	int			soundPos2;
	int			soundLoop;
	gentity_t	*parent;
	gentity_t	*nextTrain;
	gentity_t	*prevTrain;
	vec3_t		pos1, pos2;

	char		*message;

	int			timestamp;		// body queue sinking, etc

	float		angle;			// set in editor, -1 = up, -2 = down
	char		*target;
	char		*targetname;
	char		*team;
	char		*targetShaderName;
	char		*targetShaderNewName;
	gentity_t	*target_ent;

	float		speed;
	vec3_t		movedir;

	int			nextthink;
	void		(*think)(gentity_t *self);
	void		(*reached)(gentity_t *self);	// movers call this when hitting endpoint
	void		(*blocked)(gentity_t *self, gentity_t *other);
	void		(*touch)(gentity_t *self, gentity_t *other, trace_t *trace);
	void		(*use)(gentity_t *self, gentity_t *other, gentity_t *activator);
	void		(*pain)(gentity_t *self, gentity_t *attacker, int damage);
	void		(*die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

	// Mino: Not sure if these go here, but there are definitely 4 pointers somewhere here.
	void* _unknown_p1;
	void* _unknown_p2;
	void* _unknown_p3;
	void* _unknown_p4;
	//int			pain_debounce_time;
	int			fly_sound_debounce_time;	// wind tunnel
	int			last_move_time;
    
	int			health;

	qboolean	takedamage;

	int			damage;
	int			splashDamage;	// quad will increase this without increasing radius
	int			splashRadius;
	int			methodOfDeath;
	int			splashMethodOfDeath;

	int			count;

	gentity_t	*chain;
	gentity_t	*enemy;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team
//#ifdef MISSIONPACK
	int			kamikazeTime;
	int			kamikazeShockTime;
//#endif
	int			watertype;
	int			waterlevel;

	int			noise_index;

	// timing variables
	float		wait;
	float		random;

	gitem_t		*item;			// for bonus items

	int32_t _unknown3;
    void* _unknown_p5;
    void* _unknown_p6;
};

typedef struct {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int			gentitySize;
	int			num_entities;		// current number, <= MAX_GENTITIES

	int			warmupTime;			// restart match at this time

	fileHandle_t	logFile;

	// store latched cvars here that we want to get at often
	int			maxclients;

	int			time;					// in msec
	int			previousTime;			// so movers can back up when blocked

	int			startTime;				// level.time the map was started

	int	unknown_int1; // Mino: Q3 has frameNum before. I think it's gone?
	int unknown_int2;

	int			teamScores[TEAM_NUM_TEAMS];
	int			lastTeamLocationTime;		// last time of client team location update

	qboolean	newSession;				// don't use any old session data, because
										// we changed gametype

	qboolean	restarted;				// waiting for a map_restart to fire

	int			numConnectedClients;
	int			numNonSpectatorClients;	// includes connecting clients
	int			numPlayingClients;		// connected, non-spectators
	int			sortedClients[MAX_CLIENTS];		// sorted by score
	int			follow1, follow2;		// clientNums for auto-follow spectators

	int			snd_fry;				// sound index for standing in lava

	int			warmupModificationCount;	// for detecting if g_warmup is changed

	int unknown_int3[3];

	// voting state
	char		voteString[MAX_STRING_CHARS];
	char		voteDisplayString[MAX_STRING_CHARS];
	int			voteExecuteTime;		// time the vote is executed
	int			voteTime;				// level.time vote was called

	int			voteYes;
	int			voteNo;
	int			numVotingClients;		// set by CalculateRanks

	// team voting state
	char		teamVoteString[2][MAX_STRING_CHARS];
	int			teamVoteTime[2];		// level.time vote was called
	int			teamVoteYes[2];
	int			teamVoteNo[2];
	int			numteamVotingClients[2];// set by CalculateRanks

	// spawn variables
	qboolean	spawning;				// the G_Spawn*() functions are valid
	int			numSpawnVars;
	char		*spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int			numSpawnVarChars;
	char		spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int			intermissionQueued;		// intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay
	int			intermissiontime;		// time the intermission was started
	char		*changemap;
	qboolean	readyToExit;			// at least one client wants to exit
	int			exitTime;
	vec3_t		intermission_origin;	// also used for spectator spawns
	vec3_t		intermission_angle;

	qboolean	locationLinked;			// target_locations get linked
	gentity_t	*locationHead;			// head of the location list
	int			bodyQueIndex;			// dead bodies
	gentity_t	*bodyQue[BODY_QUEUE_SIZE];
	int			portalSequence;

	uint8_t 	unknown1[10144]; // Mino: A shit ton of new stuff. TODO: x86-outdated.
} level_locals_t;

// Some extra stuff that's not in the Q3 source. These are the commands you
// get when you type ? in the console. The array has a sentinel struct, so
// check "cmd" == NULL.
typedef struct {
	privileges_t needed_privileges;
	int unknown1;
	char* cmd; // The command name, e.g. "tempban".
	void (*admin_func)(gentity_t* ent);
	int unknown2;
	int unknown3;
	char* description; // Command description that gets printed when you do "?".
} adminCmd_t;

// A pointer to the qagame module in memory and its entry point.
extern void* qagame;
extern void* qagame_dllentry;

// Additional key struct pointers.
extern server_t* sv;
extern serverStatic_t* svs;
extern gentity_t* g_entities;
extern level_locals_t* level;
// Cvars.
extern cvar_t* sv_maxclients;

// Internal QL function pointer types.
typedef void (__cdecl *Com_Printf_ptr)(char* fmt, ...);
typedef void (__cdecl *Cmd_AddCommand_ptr)(char* cmd, void* func);
typedef char* (__cdecl *Cmd_Args_ptr)(void);
typedef char* (__cdecl *Cmd_Argv_ptr)(int arg);
typedef int (__cdecl *Cmd_Argc_ptr)(void);
typedef char* (__cdecl *Cmd_ArgsFrom_ptr)(int arg);
typedef void (__cdecl *Cmd_TokenizeString_ptr)(const char* text_in);
typedef void (__cdecl *Cbuf_ExecuteText_ptr)(int exec_when, const char* text);
typedef cvar_t* (__cdecl *Cvar_FindVar_ptr)(const char* var_name);
typedef cvar_t* (__cdecl *Cvar_Get_ptr)(const char* var_name, const char* var_value, int flags);
typedef cvar_t* (__cdecl *Cvar_GetLimit_ptr)(const char* var_name, const char* var_value, const char* min, const char* max, int flag);
typedef cvar_t* (__cdecl *Cvar_Set2_ptr)(const char* var_name, const char* value, qboolean force);
typedef void (__cdecl *SV_SendServerCommand_ptr)(client_t* cl, const char* fmt, ...);
typedef void (__cdecl *SV_ExecuteClientCommand_ptr)(client_t* cl, const char* s, qboolean clientOK);
typedef void (__cdecl *SV_ClientEnterWorld_ptr)(client_t *client, usercmd_t *cmd);
typedef void (__cdecl *SV_Shutdown_ptr)(char* finalmsg);
typedef void (__cdecl *SV_Map_f_ptr)(void);
typedef void (__cdecl *SV_ClientThink_ptr)(client_t* cl, usercmd_t* cmd);
typedef void (__cdecl *SV_SetConfigstring_ptr)(int index, const char* value);
typedef void (__cdecl *SV_GetConfigstring_ptr)(int index, char* buffer, int bufferSize);
typedef void (__cdecl *SV_DropClient_ptr)(client_t* drop, const char* reason);
typedef void (__cdecl *FS_Startup_ptr)(const char* gameName);
typedef void (__cdecl *Sys_SetModuleOffset_ptr)(char* moduleName, void* offset);
typedef void (__cdecl *SV_LinkEntity_ptr)(sharedEntity_t* gEnt);
// VM functions.
typedef void (__cdecl *G_RunFrame_ptr)(int time);
typedef void (__cdecl *G_AddEvent_ptr)(gentity_t* ent, int event, int eventParm);
typedef void (__cdecl *G_InitGame_ptr)(int levelTime, int randomSeed, int restart);
typedef int (__cdecl *CheckPrivileges_ptr)(gentity_t* ent, char* cmd);
typedef char* (__cdecl *ClientConnect_ptr)(int clientNum, qboolean firstTime, qboolean isBot);
typedef void (__cdecl *ClientDisconnect_ptr)(int clientNum);

// Some of them are initialized by Initialize(), but not all of them necessarily.
extern Com_Printf_ptr Com_Printf;
extern Cmd_AddCommand_ptr Cmd_AddCommand;
extern Cmd_Args_ptr Cmd_Args;
extern Cmd_Argv_ptr Cmd_Argv;
extern Cmd_Argc_ptr Cmd_Argc;
extern Cmd_ArgsFrom_ptr Cmd_ArgsFrom;
extern Cmd_TokenizeString_ptr Cmd_TokenizeString;
extern Cbuf_ExecuteText_ptr Cbuf_ExecuteText;
extern Cvar_FindVar_ptr Cvar_FindVar;
extern Cvar_Get_ptr Cvar_Get;
extern Cvar_GetLimit_ptr Cvar_GetLimit;
extern Cvar_Set2_ptr Cvar_Set2;
extern SV_SendServerCommand_ptr SV_SendServerCommand;
extern SV_ExecuteClientCommand_ptr SV_ExecuteClientCommand;
extern SV_ClientEnterWorld_ptr SV_ClientEnterWorld;
extern SV_Shutdown_ptr SV_Shutdown; // Used to get svs pointer.
extern SV_Map_f_ptr SV_Map_f; // Used to get Cmd_Argc
extern SV_SetConfigstring_ptr SV_SetConfigstring;
extern SV_GetConfigstring_ptr SV_GetConfigstring;
extern SV_DropClient_ptr SV_DropClient;
extern Sys_SetModuleOffset_ptr Sys_SetModuleOffset;
// VM functions.
extern G_RunFrame_ptr G_RunFrame;
extern G_AddEvent_ptr G_AddEvent;
extern G_InitGame_ptr G_InitGame;
extern CheckPrivileges_ptr CheckPrivileges;
extern ClientConnect_ptr ClientConnect;
extern ClientDisconnect_ptr ClientDisconnect;

// Server replacement functions for hooks.
void __cdecl My_Cmd_AddCommand(char* cmd, void* func);
void __cdecl My_Sys_SetModuleOffset(char* moduleName, void* offset);
#ifndef NOPY
void __cdecl My_SV_ClientEnterWorld(client_t* client, usercmd_t* cmd);
void __cdecl My_SV_SetConfigstring(int index, char* value);
void __cdecl My_SV_DropClient(client_t* drop, const char* reason);
// VM replacement functions for hooks.
void __cdecl My_G_RunFrame(int time);
void __cdecl My_G_InitGame(int levelTime, int randomSeed, int restart);
char* __cdecl My_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot);
#endif

// Custom commands added using Cmd_AddCommand during initialization.
void __cdecl SendServerCommand(void); // "cmd"
void __cdecl CenterPrint(void); // "cp"
void __cdecl RegularPrint(void); // "p"
void __cdecl Slap(void); // "slap"
void __cdecl Slay(void); // "slay"
#ifndef NOPY
// PyRcon gives the owner the ability to execute pyminqlx commands as if the
// owner executed them.
void __cdecl PyRcon(void);
// PyCommand is special. It'll serve as the handler for console commands added
// using Python. This means it can serve as the handler for a bunch of commands,
// and it'll take care of redirecting it to Python.
void __cdecl PyCommand(void);
void __cdecl RestartPython(void); // "pyrestart"
#endif

#endif /* QUAKE_COMMON_H */
