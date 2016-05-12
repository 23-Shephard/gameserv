#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define BUF_SIZE 128
#define AUC_RES_SIZE 500

int pl_count, pl_n;
int started = 0, month = 0;

struct build_f {
	int days;
	struct build_f *next;
};

enum st {
	off = 0,
	play = 1,
	end_turn = 2,
	bankrupt = -1,
};

struct player {
	int sd;
	enum st status;
	char buf[BUF_SIZE];
	int pos;
	int money;
	int material;
	int products;
	int for_prod;
	int factories;
	struct build_f *building;
};

enum bank_mode { sell, buy, do_auction, market_info, market_change };

struct request {
	int player_n;
	struct player *pl;
	int price;
	int count;
};

struct market_status {
	int level;
	int sell_n;
	int min_price;
	int buy_n;
	int max_price;
};

struct auc {
	struct request *req;
	struct auc *next;
};

typedef void (*sat_ptr)(struct request *, int, char *);

int is_number(char *str)
{
	int i;
	if (str == NULL)
		return 0;
	for (i=0; str[i]; i++) {
		if (str[i]<'0' || str[i]>'9')
			return 0;
	}
	return 1;
}

int has_string(char *buf, int len)
{
	int i;
	if (!buf)
		return 0;
	for (i=0; i<len; i++) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			return i+1;
		}
	}
	return 0;
}

void shift(struct player *p, int from)
{
	int i,j;
	for (i=0,j=from; j<p->pos; i++,j++)
		p->buf[i] = p->buf[j];
	p->pos -= from;
}

struct list {
	char *str;
	struct list *next;
};

struct list *make_list(char *buf, int n)
{
	struct list *ptr;
	int i;
	ptr = malloc(sizeof(struct list));
	ptr->str = malloc(n*sizeof(char));
	ptr->next = NULL;
	for (i=0; i<n; i++) {
		if (buf[i]!=' ' && buf[i]!='\t' && buf[i]!='\r')
			ptr->str[i] = buf[i];
		else {
			ptr->str[i] = '\0';
			if (buf[i+1]!='\0') {
				ptr->next = make_list(&buf[i+1], n-i);
				break;
			}
		}
	}
	return ptr;
}
	
int list_size(struct list *ptr)
{
	int i = 0;
	while (ptr!=NULL) {
		if (ptr->str[0]!='\0')
			i++;
		ptr = ptr->next;
	}
	return i;
}

void delete_list(struct list *p)
{
	if (p!=NULL) {
		if (p->next!=NULL)
			delete_list(p->next);
		free(p);
	}
}
	
char **make_cmd(char *buf, int n)
{
	struct list *cmd0 = make_list(buf, n);
	int len = list_size(cmd0);
	char **cmd = malloc((len+1)*sizeof(struct list));
	int i;
	struct list *p = cmd0;
	for (i=0; i<len; i++) {
		while (p->str[0]=='\0') {
			free(p->str);
			p = p->next;
		}
		cmd[i] = p->str;
		p = p->next;
	}
	cmd[len] = NULL;
	delete_list(cmd0);
	return cmd;
}

void delete_cmd(char **cmd)
{
	int i;
	for (i=0; cmd[i]; i++)
		free(cmd[i]);
}

void pl_init_all(struct player *p)
{
	int i;
	for (i=0; i<pl_n; i++) {
		p[i].status = off;
		p[i].pos = 0;
		p[i].sd = 0;
		p[i].money = 10000;
		p[i].material = 4;
		p[i].for_prod = 0;
		p[i].products = 2;
		p[i].factories = 2;
		p[i].building = NULL;
	}
}

int find_free_index(struct player *p)
{
	int i;
	for (i=0; i<pl_n; i++) {
		if (!p[i].status)
			return i;
	}
	return -1;
}

int create_listening_socket(int port)
{
	struct sockaddr_in addr;
	int ls;
	int opt = 1;
	if ((ls = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(ls, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		perror("bind");
		exit(1);
	}
	if (listen(ls, 5) == -1) {
		perror("listen");
		exit(1);
	}
	return ls;
}

void print_msg(struct player *p, const char *msg)
{
	void end(struct player *);
	if (write(p->sd, msg, strlen(msg)) == -1 && errno == EPIPE)
		end(p);
}

void notify_all(struct player *p, const char *mes)
{
	int i;
	for (i=0; i<pl_n; i++) {
		if (p[i].status != off)
			print_msg(&p[i], mes);
	}
}

char *how_many_players(void)
{
	static char status[42];
	sprintf(status, "Now there are %d/%d players\n", pl_count, pl_n);
	return status;
}

void end(struct player *p)
{
	struct build_f *del_build(struct build_f *);
	p->pos = 0;
	if (p->status != bankrupt)
		pl_count--;
	if (p->building)
		while (p->building)
			p->building = del_build(p->building);
	p->status = off;
	shutdown(p->sd, 2);
	close(p->sd);
}

void greet(struct player *p, int k)
{
	char *n = how_many_players();
	char *str = malloc(42);
	sprintf(str, "Your number is %d\n", k+1);
	print_msg(&p[k], "Welcome to my game!\n");
	print_msg(&p[k], str);
	print_msg(&p[k], "Type 'help' to get help\n");
	notify_all(p, n);
	free(str);
}

void print_market(struct player *p, int k, struct market_status *m)
{
	char *str=malloc(150);
	sprintf(str, "Current month is %%%d\n"
		"Players still active:\n"
		"%% \t     %d\n"
		"bank sells: items min.price\n"
		"%% \t     %d       %d\n"
		"bank buys: items max.price\n"
		"%% \t     %d       %d\n",
		 month, pl_count, m->sell_n, m->min_price,
		 m->buy_n, m->max_price);
	print_msg(&p[k], str);
	free(str);
}

int building_factories(struct build_f *ptr)
{
	int k;
	for (k=0; ptr; k++)
		ptr = ptr->next;
	return k;
}

void print_player(struct player *p, int k, char **cmd)
{
	int i;
	if (cmd[1]==NULL || !is_number(cmd[1]) || cmd[2]!=NULL) {
		print_msg(&p[k], "Syntax error!\n");
		return;
	}
	i = atoi(cmd[1]);
	if (1<=i && i<=pl_n && p[i-1].status!=off) {
		char *str = malloc(256);
		if (p[i-1].status == bankrupt)
			sprintf(str, "Player %d is a bankrupt\n", i);
		else
			sprintf(str, "Player %d has: dollars product "
				"material factories building; makes turn\n"
				"%% \t      %d \t %d \t %d\t  %d \t   %d"
				"\t     %s \n",
				i, p[i-1].money, p[i-1].products,
				p[i-1].material, p[i-1].factories,
				building_factories(p[i-1].building),
				(p[i-1].status==play) ? "yes" : "no");
		print_msg(&p[k], str);
		free(str);
	} else {
		print_msg(&p[k], "There is no such player\n");
	}
}	

void request_prod(struct player *p, int k, char **cmd)
{
	int i;
	if (cmd[1]==NULL || !is_number(cmd[1]) || cmd[2]!=NULL
		|| (i = atoi(cmd[1])) < 0) {
		print_msg(&p[k], "Syntax error!\n");
		return;
	}
	if (p[k].money < 2000*i) {
		print_msg(&p[k], "Not enough money\n");
		return;
	}
	if (p[k].material < i) {
		print_msg(&p[k], "Not enough material\n");
		return;
	}
	if (p[k].factories-p[k].for_prod < i) {
		print_msg(&p[k], "Not enough factories\n");
		return;
	}
	p[k].money -= 2000*i;
	p[k].material -= i;
	p[k].for_prod += i;
}

struct auc *accept_request(struct auc *queue, struct request *r,
	enum bank_mode mode, struct market_status *st)
{
	if ((mode == sell && r->price > st->max_price)
		|| (mode == buy && r->price < st->min_price)) {
		if (mode == sell)
			r->pl->products += r->count;
		else
			r->pl->money += r->price * r->count;
		print_msg(r->pl, "Bank doesn't accept it\n");
		free(r);
		return queue;
	}
	print_msg(r->pl, "Accepted.\n");
	if (queue == NULL) {
		queue = malloc(sizeof(struct auc));
		queue->req = r;
		queue->next = NULL;
	} else if (mode == sell && r->price > queue->req->price )
		queue = accept_request(queue->next, r, sell, st);
	else if (mode == buy && r->price < queue->req->price )
		queue = accept_request(queue->next, r, buy, st);
	else {
		struct auc *tmp = malloc(sizeof(struct auc));
		tmp->next = queue;
		tmp->req = r;
		return tmp;
	}
	return queue;
}
		
void change_level(struct market_status *old)
{
	const int level_change[5][5] = {
		{ 4, 4, 2, 1, 1 },
		{ 3, 4, 3, 1, 1 },
		{ 1, 3, 4, 3, 1 },
		{ 1, 1, 3, 4, 3 },
		{ 1, 1, 2, 4, 4 },
	};
	int r = 1 + (int)(12.0*rand()/(RAND_MAX+1.0));
	int i, sum;
	for (i=0,sum=0; sum<r; i++)
		sum += level_change[old->level-1][i];
	(month>1) ? (old->level = i-1) : (old->level = 3);
	switch (old->level) {
	case 1:
		old->sell_n = pl_count;
		old->min_price = 800;
		old->buy_n = 3*pl_count;
		old->max_price = 6500;
		break;
	case 2:
		old->sell_n = (int)(1.5*pl_count);
		old->min_price = 650;
		old->buy_n = (int)(2.5*pl_count);
		old->max_price = 6000;
		break;
	case 3:
		old->sell_n = 2*pl_count;
		old->min_price = 500;
		old->buy_n = 2*pl_count;
		old->max_price = 5500;
		break;
	case 4:
		old->sell_n = (int)(2.5*pl_count);
		old->min_price = 400;
		old->buy_n = (int)(1.5*pl_count);
		old->max_price = 5000;
		break;
	case 5:
		old->sell_n = 3*pl_count;
		old->min_price = 300;
		old->buy_n = pl_count;
		old->max_price = 4500;
		break;
	}
}

struct auc *delete_request(struct auc *ptr)
{
	struct auc *tmp;
	tmp = ptr->next;
	free(ptr->req);
	free(ptr);
	return tmp;
}

void satisfy_buy(struct request *req, int ammount, char *auc_res)
{
	if (ammount>0) {
		char *str = malloc(100);
		sprintf(str, "# Player %d bought %d materials "
			"and spent %d dollars\n",
			req->player_n, ammount, req->price*ammount);
		strcat(auc_res, str);
		free(str);
	}
	req->pl->material += ammount;
	req->pl->money += (req->count-ammount) * req->price;
}

void satisfy_sell(struct request *req, int ammount, char *auc_res)
{
	if (ammount>0) {
		char *str = malloc(100);
		sprintf(str, "# Player %d sold %d products "
			"and gained %d dollars\n",
			req->player_n, ammount, req->price*ammount);
		strcat(auc_res, str);
		free(str);
	}
	req->pl->money += ammount * req->price;
	req->pl->products += req->count - ammount;
}


struct auc *auc_chance(struct auc *queue, int possible_deals,
	int participants, sat_ptr satisfy, char *auc_res)
{
	int r = (int)((float)(participants)*rand()/(RAND_MAX+1.0));
	struct auc *tmp = queue;
	int i;
	for (i=0; i<r; i++)
		tmp = tmp->next;
	if (tmp->req->count <= possible_deals) {
		(*satisfy)(tmp->req, tmp->req->count, auc_res);
		possible_deals -= tmp->req->count;
	} else {
		(*satisfy)(tmp->req, possible_deals, auc_res);
		possible_deals = 0;
	}
	if (r == 0) /*the begining of the queue*/
		queue = delete_request(queue);
	else
		tmp = delete_request(tmp);
	if (possible_deals > 0) {
		return auc_chance(queue, possible_deals, participants-1,
			satisfy, auc_res);
	} else {
		while (queue) {
			(*satisfy)(queue->req, 0, NULL);
			queue = delete_request(queue);
		}
	}
	return queue;
}
	
struct auc *auction(struct auc *queue, int possible_deals,
	sat_ptr satisfy, char *auc_res)
{
	int best, participants, prod_n, i;
	struct auc *tmp = queue;
	if (queue == NULL)
		return queue;
	if (possible_deals == 0) {
		while (queue) {
			(*satisfy)(queue->req, 0, NULL);
			queue = delete_request(queue);
		}
		return queue;
	}
	best = queue->req->price;
	participants = prod_n = 0;
	while (tmp && tmp->req->price == best) {
		participants++;
		prod_n += tmp->req->count;
		tmp = tmp->next;
	}
	if (prod_n <= possible_deals) {
		for (i=0; i<participants; i++) {
			(*satisfy)(queue->req, queue->req->count, auc_res);
			prod_n -= queue->req->count;
			possible_deals -= queue->req->count;
			queue = delete_request(queue);
		}
		return auction(queue, possible_deals, satisfy, auc_res);
	} else {
		return auc_chance(queue, prod_n, participants,
			satisfy, auc_res);
	}
}

void bank(enum bank_mode mode, struct player *p, int k, struct request *req)
{
	static struct market_status st;
	static struct auc *for_selling = NULL, *for_buying = NULL;
	static char auc_res[AUC_RES_SIZE];
	auc_res[0] = '\0';
	switch (mode) {
	case sell:
		for_selling = accept_request(for_selling, req, sell, &st);
		break;
	case buy:
		for_buying = accept_request(for_buying, req, buy, &st);
		break;
	case market_change:
		change_level(&st);
		break;
	case market_info:
		print_market(p, k, &st);
		break;
	case do_auction:
		for_selling = auction(for_selling, st.buy_n,
			satisfy_sell, auc_res);
		for_buying = auction(for_buying, st.sell_n,
			satisfy_buy, auc_res);
		notify_all(p, auc_res);
		break;
	}
}

void request_for_bank(struct player *p, int k, char **cmd)
{
	struct request *r;
	int count, price;
	if (cmd[1]==NULL || !is_number(cmd[1]) || cmd[2]==NULL
		|| !is_number(cmd[2]) || cmd[3]!=NULL
		|| (count = atoi(cmd[1])) < 0
		|| (price = atoi(cmd[2])) < 0) {
		print_msg(&p[k], "Syntax error!\n");
		return;
	}
	r = malloc(sizeof(struct request));
	r->price = price;
	r->count = count;
	r->pl = &p[k];
	r->player_n = k+1;
	if (cmd[0][0]=='s') {
		if (p[k].products >= r->count) {
			p[k].products -= r->count;
			bank(sell, p, k, r);
		} else {
			print_msg(&p[k], "Not enough product\n");
			free(r);
		}
	} else {
		if (p[k].money >= r->price) {
			p[k].money -= r->price * r->count;
			bank(buy, p, k, r);
		} else {
			print_msg(&p[k], "Not enough money\n");
			free(r);
		}
	}
}

struct build_f *build(struct player *p, struct build_f *building)
{
	if (!building) {
		if (p->money >= 2500) {
			building = malloc(sizeof(struct build_f));
			building->days = 5;
			p->money -= 2500;
			building->next = NULL;
		} else {
			print_msg(p, "Not enough money\n");
		}
	} else
		build(p, building->next);
	return building;
}

void execute(struct player *p, int k, char **cmd)
{
	if (strcmp(cmd[0], "help") == 0) {
		print_msg(&p[k], "market \t\t information about market \n"
			"player N \t information about player N\n"
			"prod N \t\t make N units of product\n"
			"buy N P \t try to buy N units of material "
				"for P dollars \n"
			"sell N P \t try to sell N units of product "
				"for P dollars \n"
			"build \t\t start building new factory\n"
			"turn \t\t finish all the actions for this turn\n"
			"help \t\t get help about commands\n");
		return;
	}
	if (started) {
		if (p[k].status == play) {
			if (strcmp(cmd[0], "prod") == 0) {
				request_prod(p, k, cmd);
				return;
			}
			else if (strcmp(cmd[0], "buy") == 0
					|| strcmp(cmd[0], "sell") == 0) {
				request_for_bank(p, k, cmd);
				return;
			}
			else if (strcmp(cmd[0], "build") == 0) {
				p[k].building = build(&p[k], p[k].building);
				return;
			}
			else if (strcmp(cmd[0], "turn") == 0) {
				p[k].status = end_turn;
				return;
			}
		}
		if (strcmp(cmd[0], "market") == 0)
			bank(market_info, p, k, NULL);
		else if (strcmp(cmd[0], "player") == 0)
			print_player(p, k, cmd);
		else
			print_msg(&p[k], "Illegal command\n");
	} else {
		print_msg(&p[k], "Game hasn't begun\n");
	}
}

int recieve(struct player *p)
{
	int rc, buflen;
	char *ptr;
	ptr = p->buf + p->pos;
	buflen = BUF_SIZE - p->pos;
	rc = read(p->sd, ptr, buflen);
	if (rc != -1)
		p->pos += rc;
	return rc;
}
	
void check_buf(struct player *p)
{
	if (p->pos+10 > BUF_SIZE)
		p->pos = 0;
}

struct build_f *del_build(struct build_f *ptr)
{
	struct build_f *tmp;
	if (ptr) {
		tmp = ptr->next;
		free(ptr);
		ptr = tmp;
	}
	return ptr;
}

void handle_building(struct player *p)
{
	struct build_f **t = &p->building;
	while (*t) {
		(*t)->days--;
		if ((*t)->days == 1)
			p->money -= 2500;
		if ((*t)->days == 0) {
			p->factories++;
			*t = del_build(*t);
		} else {
			t = &((*t)->next);
		}
	}
} 

void new_month(struct player *p)
{
	int i;
	char *mon = malloc(64);
	sprintf(mon, "The month %d has begun\n", ++month);
	notify_all(p, mon);
	free(mon);	
	bank(market_change, NULL, 0, NULL);
	for (i=0; i<pl_n; i++) {
		if (p[i].status == end_turn)
			p[i].status = play;
	}
}
	
void congratulate_winner(struct player *p)
{
	int i;
	for (i=0; i<pl_n; i++) {
		if (p[i].status == play || p[i].status == end_turn) {
			char str[50];
			sprintf(str, "Player %d has won the game."
				" Congratulations!\n", i+1);
			notify_all(p, str);
			notify_all(p, "See you again!;)\n");
			int j;
			for (j=0; j<pl_n; j++) {
				if (p[j].status) {
					shutdown(p[j].sd, 2);
					close(p[j].sd);
				}
			}
			return;
		}
	} 
}

void reset_game(struct player *p)
{
	started = month = pl_count = 0;
	pl_init_all(p);
}

void end_month(struct player *p)
{
	int i;
	bank(do_auction, p, 0, NULL);
	for (i=0; i<pl_n; i++) {
		if (p[i].status == end_turn) {
			p[i].products += p[i].for_prod;
			p[i].for_prod = 0;
			p[i].money -= 300*p[i].material + 500*p[i].products
				+ 1000*p[i].factories;
			handle_building(&p[i]);
			if (p[i].money < 0) {
				char *str = malloc(50);
				sprintf(str, "Player %d has gone bust\n",
					i+1);
				p[i].status = bankrupt;
				print_msg(&p[i], "YOU ARE BANKRUPT!!!!\n");
				pl_count--;
				notify_all(p, str);
				free(str);
			}
		}
	}
	if (pl_count == 0) {
		notify_all(p, "Game over :(\n");
		for (i=0; i<pl_n; i++)
			if (p[i].status==bankrupt)
				end(&p[i]);
		reset_game(p);
		return;
	} else if (pl_count == 1 && started == 1) {
		congratulate_winner(p);
		reset_game(p);
		return;
	}
	new_month(p);
}

/*returns -1 if player left the game*/
int something_to_do_with(struct player *p, int k)
{
	int len;
	if ((len = recieve(&p[k])) == 0) {
		end(&p[k]);
		return -1;
	}
	len = has_string(p[k].buf, p[k].pos);
	if (len) {
		char **cmd = make_cmd(p[k].buf, len);
		shift(&p[k], len);
		if (*cmd) {
			execute(p, k, cmd);
			delete_cmd(cmd);
		}
		free(cmd);
	} else {
		check_buf(&p[k]);
	}
	return 0;
}

void reject(int fd)
{
	const char mes[] = "Sorry, the game has already begun :(\n";
	write(fd, mes, sizeof(mes)-1);
	shutdown(fd, 2);
	close(fd);
}
	
int load_set(int ls, struct player *p, fd_set *set)
{
	int max_d = ls;
	int i;
	FD_ZERO(set);
	FD_SET(ls, set);
	for (i=0; i<pl_n; i++) {
		if (p[i].status != off) {
			FD_SET(p[i].sd, set);
			if (p[i].sd > max_d)
				max_d = p[i].sd;
		}
	}
	return max_d;
}

void handle_guest(int ls, struct player *p)
{
	int fd;
	if ((fd = accept(ls, NULL, NULL)) != -1) {
		if (!started) {
			int first = find_free_index(p);
			p[first].sd = fd;
			p[first].status = play;
			pl_count++;
			greet(p, first);
			if (pl_count == pl_n) {
				started = 1;
				notify_all(p, "Let's play\n");
				new_month(p);
			}
		} else {
			reject(fd);
		}
	}		
}

void handle_players(struct player *p, fd_set *set)
{
	int i;
	for (i=0; i<pl_n; i++) {
		if (FD_ISSET(p[i].sd, set)) {
			if (something_to_do_with(p, i) == -1) {
				char *msg = how_many_players();
				notify_all(p, msg);
			}
		}
	}
}

int main(int argc, char **argv)
{
	int port, ls;
	struct player *players;
	srand(time(NULL));
	signal(SIGPIPE, SIG_IGN);
	if (argc < 3 || !is_number(argv[1]) || !is_number(argv[2])
		|| (pl_n = atoi(argv[1])) < 0 || pl_n > 1000
		|| (port = atoi(argv[2])) < 1) {
		fprintf(stderr, "Usage: ./server players port\n");
		exit(1);
	}
	players = malloc(pl_n*sizeof(struct player));
	pl_init_all(players);
	ls = create_listening_socket(port);
	pl_count = 0;
	while (1) {
		int i;
		fd_set read_fds;
		int max_d = load_set(ls, players, &read_fds);
		if (select(max_d+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}
		if (FD_ISSET(ls, &read_fds))
			handle_guest(ls, players);
		handle_players(players, &read_fds);
		if (pl_count == 0) {
			reset_game(players);
			continue;
		}
		
		for (i=0; i<pl_n; i++) {
			if (players[i].status == play)
				break;
		}
		if (i == pl_n)
			end_month(players);
	}
	return 0;
}
