#include <stdio.h>            
#include <stdlib.h> 
#include <string.h>

typedef struct node {
	char  name[64];      
	char  type;
	struct node *child, *sibling, *parent;
} NODE;


NODE *root; 
NODE *cwd;
char *cmd[] = {"mkdir", "rmdir", "ls", "cd", "pwd", "creat", "rm",
"reload", "save", "quit", 0};  

int initialize() {
	root = (NODE *)malloc(sizeof(NODE));
	strcpy(root->name, "/");
	root->parent = root;
	root->sibling = NULL;
	root->child = NULL;
	root->type = 'D';
	cwd = root;
	
	printf("Filesystem initialized!\n");
}

NODE* create_node(char name[], char type)
{
	NODE *node = (NODE*)malloc(sizeof(NODE));
	strcpy(node->name, name);
	node->type = type;
	node->child = node->parent = node->sibling = NULL;
	return node;
}

void remove_node(NODE* to_remove)
{
	NODE* parent = to_remove->parent;
	if(parent->child == to_remove) // child is the first child of the parent
	{
		if(to_remove->sibling == NULL) // to_remove has no siblings
		{
			// good to remove
			free(to_remove);
			parent->child = NULL;
			return;
		}
		else // has siblings
		{
			parent->child = to_remove->sibling;
			free(to_remove);
			return;
		}
	}
	else
	{
		NODE *child = parent->child;
		while(child->sibling != NULL)
		{
			if(child->sibling == to_remove) // we found the node to remove
			{
				// unlink to_remove
				child->sibling = to_remove->sibling;
				free(to_remove);
				return;
			}
			// move to the next child
			child = child->sibling;
		}
	}
}

void print_node(NODE* node)
{
	printf("%c %s\n", node->type, node->name);
}


void insert_child(NODE *parent, NODE *to_insert)
{
	if(!parent->child)
	{
		parent->child = to_insert;
	}
	else
	{
		NODE *pCur = parent->child;
		while(pCur->sibling)
		{
			pCur = pCur->sibling;
		}
		pCur->sibling = to_insert;
	}
	to_insert->parent = parent;
}

NODE* find_node(NODE* cur_root, char *target)
{
	if(cur_root == NULL) // we couldn't find the target
	{
		return NULL; 
	}
	else if(strcmp(cur_root->name, target) == 0) // we found it
	{
		return cur_root;
	}
	else
	{
		NODE* node = find_node(cur_root->child, target); // search the children
		if(node != NULL)
		{
			return node;
		}
		else // search the siblings
		{
			return find_node(cur_root->sibling, target);
		}
	}
}

NODE* find_node_master(NODE *c_root, char* dirname)
{
	NODE *cur_root = c_root;
	char *tok = strtok(dirname, "/");// gets the first part of path
	if(tok != NULL && strcmp(tok, "\0") == 0)
	{
		return root;
	}
	if (tok != NULL && strcmp(tok, "."))
	{
		while(tok) // while the tok is not null
		{
			cur_root = find_node(cur_root, tok);
			tok = strtok(NULL, "/");
			if(cur_root == NULL)
			{
				// kills the process early
				break;
			}
		}
	}
	// at this point, we should have the desired 
	// will return a null pointer if not found
	return cur_root;
}

int find_command(char *user_command, char *cmds[])
{
    int i = 0;
    while(cmds[i])
    {
        if (strcmp(user_command, cmds[i])==0)
        {
            return i;
        }
        i++;
    }
    return -1;
}


void parse_path(char *path, char* dirname, char* basename)
{
	// parses a path and divides it into dirname and basename
	char path_cpy[64] = "\0";
	strcpy(path_cpy, path);
	strcpy(dirname, path);
	int count = 0;
	char* tok = strtok(path_cpy, "/");
	char *prev = tok;
	while(tok)
	{
		prev = tok;
		tok = strtok(NULL, "/");
		count++;
	}


	if(prev == NULL)
	{
		return;
	}
	strcpy(basename, prev);


	// removes trailing / 
	if(count >= 2)
	{
		dirname[strlen(dirname) - strlen(basename)-1] = '\0';
	}
	else
	{
		dirname[strlen(dirname) - strlen(basename)] = '\0';
	}
}

void mkdir(char* pathname)
{
	char basename[64] = "\0";
	char dirname[64] = "\0";
	NODE* node = NULL;
	parse_path(pathname, dirname, basename);
	if(strcmp(pathname, "/") == 0)
	{
		printf("Cannot create already existing root dir!");
		return;
	}
	if(dirname[0] == '/')
	{
		// we are dealing with absolute path
		node = find_node_master(root, dirname);
	}
	else
	{
		// relative path
		if(strcmp(dirname, "\0")==0)
		{
			// we are currently in the working directory
			node = cwd;
		}
		else
		{
			node = find_node_master(cwd, dirname);
		}

	}


	if(node == NULL)
	{
		printf("Error: directory does not exist\n");
		return;
	}
	else
	{
		// need to go down one level to see if basename node already exists
		NODE* parent = node;
		node = node->child;
		while(node != NULL)
		{
			if(strcmp(node->name, basename)==0 && node->type == 'D')
			{
				printf("Error: %s already exists!\n", basename);
				return;
			}
			node = node->sibling;
		}
		// at this point, we know we can make the dir
		insert_child(parent, create_node(basename, 'D'));
		//printf("Directory created!\n");
	}
}

void rmdir(char *pathname)
{
	NODE* node = NULL;
	if(pathname[0] == '/')
	{
		// we are dealing with absolute path
		node = find_node_master(root, pathname);
	}
	else
	{
		// relative path
		if(strcmp(pathname, "\0")==0)
		{
			// we are currently in the working directory
			node = cwd;
		}
		else
		{
			node = find_node_master(cwd, pathname);
		}

	}

	if(node == NULL)
	{
		printf("Error: DIR does not exist!\n");
		return;
	}

	// we need to check if the directory is empty
	if(node->child != NULL)
	{
		printf("Error: DIR is not empty!\n");
		return;
	}
	else
	{
		if(strcmp(node->name, "/") == 0 || node->type == 'F')
		{
			printf("Error: DIR does not exist!\n");
			return;
		}
		else
		{
			// we are good to delete
			if(node->parent->child == node)
			{
				// the node we're deleting is the parents first child.
				node->parent->child = node->sibling;
				free(node);
				return;
			}

			// first child
			NODE* cur = node->parent->child;
			NODE* prev  = NULL;
			//were deleting a sibling
			while(cur != node && cur != NULL)
			{
				prev = cur;
				cur = cur->sibling;
			}

			// relink the list and then delete
			prev->sibling = node->sibling;
			free(node);
		}
	}
}

void ls(char *pathname)
{
	NODE *parent;
	if(strcmp(pathname, "\0") == 0)
	{
		// use cwd
		parent = cwd;
	}
	else
	{
		// use the direct path
		parent = find_node_master(root, pathname);
	}

	if(parent == NULL)
	{
		// we didn't find the path
		printf("Error: path does not exist!\n");
		return;
	}
	else
	{
		// get the child node
		NODE* child = parent->child;
		if(child == NULL)
		{
			// the directory is empty (has no children)
			//printf("Empty directory\n");
			return;
		}
		else
		{
			// print out all the contents
			while(child!=NULL)
			{
				print_node(child);
				child = child->sibling;
			}
		}
	}
}

void cd(char *path)
{
	if(strcmp(path, "..")==0)
	{
		// we just need to go up one directory
		cwd = cwd->parent;
		return;
	}
	else if(strcmp(path, "\0")==0)
	{
		// we need to change to the root
		cwd = root;
		return;
	}
	else
	{
		NODE* node = NULL;
		// there is a path specified
		if(path[0] == '/')
		{
			node = find_node_master(root, path);	
			//printf("node found is %s", node->name);
		}
		else
		{
			node = find_node_master(cwd, path);
		}
		if(node == NULL || node->type == 'F')
		{
			printf("Error: could not change to %s\n", path);
			return;
		}
		cwd = node;
	}
}

void pwd()
{
	NODE* cur = cwd;
	char path[256] = "";
    char *result;
	if(cur == root)
	{
		printf("/\n");
		return;
	}
	while(cur != root)
	{
		char* name = cur->name;
		size_t length = strlen(path) + strlen(name) + 2;
		result = malloc(length);
		if(result == NULL)
		{
			printf("Error: failed to allocate mem\n");
		}
		strcpy(result, "/");
		strcat(result, name);
		strcat(result, path);
		cur = cur->parent;
		strcpy(path, result);
		free(result);
	}
	printf("%s\n", path);
}

void creat(char *pathname)
{
	char basename[64] = "\0";
	char dirname[64] = "\0";
	parse_path(pathname, dirname, basename);
	NODE* node = NULL;
	if(pathname == NULL)
	{
		return;
	}
	if(dirname[0] == '/')
	{
		// we are dealing with absolute path
		node = find_node_master(root, dirname);
	}
	else
	{
		// relative path
		if(strcmp(dirname, "\0")==0)
		{
			// we are currently in the working directory
			node = cwd;
		}
		else
		{
			node = find_node_master(cwd, dirname);
		}
	}


	if(node == NULL)
	{
		printf("Error: directory path does not exist!\n");
		return;
	}
	else
	{
		// need to go down one level to see if basename file already exists
		NODE* parent = node;
		node = node->child;
		while(node != NULL)
		{
			if(strcmp(node->name, basename)==0 && node->type == 'F')
			{
				printf("Error: basename file already exists!\n");
				return;
			}
			node = node->sibling;
		}
		// at this point, we know we can make the file
		insert_child(parent, create_node(basename, 'F'));
		//printf("File created!\n");
	}
}

void rm(char *pathname)
{
	NODE* node = NULL;
	if(pathname[0] == '/')
	{
		// we are dealing with absolute path
		node = find_node_master(root, pathname);
	}
	else
	{
		// relative path
		if(strcmp(pathname, ".")==0)
		{
			// we are currently in the working directory
			node = cwd;
		}
		else
		{
			node = find_node_master(cwd, pathname);
		}

	}

	if(node == NULL)
	{
		printf("Error: file to remove does not exist!\n");
		return;
	}

	// make sure we are actually deleting a file
	if(strcmp(node->name, "/") == 0 || node->type == 'D')
	{
		printf("Error: cannot remove (not a file)!\n");
		return;
	}
	else
	{
		// we are good to delete
		if(node->parent->child == node)
		{
			// the node we're deleting is the parents first child.
			node->parent->child = node->sibling;
			free(node);
			return;
		}

		// first child
		NODE* cur = node->parent->child;
		NODE* prev  = NULL;
		//were deleting a sibling
		while(cur != node && cur != NULL)
		{
			prev = cur;
			cur = cur->sibling;
		}

		// relink the list and then delete
		prev->sibling = node->sibling;
		free(node);
	}
}
void find_path(NODE* node, char* path)
{
	NODE* cur = node;
    char *result;
	if(cur == root)
	{
		strcpy(path, "/");
		return;
	}
	while(cur != root)
	{
		char* name = cur->name;
		size_t length = strlen(path) + strlen(name) + 2;
		result = malloc(length);
		if(result == NULL)
		{
			printf("Error: failed to allocate mem\n");
		}
		strcpy(result, "/");
		strcat(result, name);
		strcat(result, path);
		cur = cur->parent;
		strcpy(path, result);
		free(result);
	}
}
void save_helper(NODE *cur, FILE* fp)
{
	// print depth first
	if(cur != NULL)
	{
		char path[256] = "";
		find_path(cur, path);
		fprintf(fp, "%c %s\n", cur->type, path);
		//printf("%c %s\n", cur->type, path);
		save_helper(cur->child, fp);
	}	
	
	// then print siblings
	if(cur != NULL && cur->sibling != NULL)
	{
		save_helper(cur->sibling, fp);
	}
}

void save(char* filename)
{
	FILE *fp = fopen(filename, "w+"); // open a file stream
	save_helper(root, fp); // run the recusive save function
	fclose(fp); // close file stream when done
}

void reload(char*filename)
{
	FILE *fp = fopen(filename, "r"); // open a file stream
	if(fp == NULL)
	{
		printf("Error: filename does not exist!\n");
		return;
	}
	char line[128] = "";
	char type = '\0';
	while(!feof(fp))
	{
		if(fgets(line, sizeof(line) -1, fp)== NULL)
		{
			break;
		}
		char* tok = strtok(line, "\n");
		type = tok[0];
		if(type == 'D' && strcmp(tok+2, "/") != 0)
		{
			// we need to make the dir
			mkdir(tok+2);
		}
		else if(type == 'F')
		{
			// we need to make the file
			creat(tok+2);
		}
	}
	
	fclose(fp); // close file stream when done
}

void parse_input(char*input, char*cmd, char*path)
{
	char* tok = strtok(input, " ");
	if(tok != NULL)
	{
		strcpy(cmd, tok);
		tok = strtok(NULL, " ");
		if(tok != NULL)
		{
			strcpy(path, tok);
		}
	}
}
void evaluate_command(int index, char*arg)
{
    switch(index)
    {
        case 0 :{ 
			mkdir(arg);
			break;
			}
        case 1 : {
			rmdir(arg);
			break;}
        case 2 : {
			ls(arg);
			break;
			}
		case 3 : {
			cd(arg);
			break;
			}
        case 4 : {
			pwd();
			break;
			}
        case 5 : {
			creat(arg);
			break;
			}
        case 6: {
			rm(arg);
			break;
			}
		case 7: {
			reload(arg);
			break;
			}
        case 8: {
			save(arg);
			break;
			}
        default: {
			printf("Command does not exist!\n");
			break;
			}
    }
}
int main() {
	int cmd_index = 0;
	char user_input[64];
	char user_cmd[12];
	char path[52];
	initialize();
	char basename[50];
	char dirname[50];

	while(1) {
		strcpy(path, "\0");
		strcpy(basename, "\0");
		strcpy(dirname, "\0");
		printf("Enter command: ");
		scanf("%63[^\n]", user_input);

		getchar(); // get the newline left over
		parse_input(user_input, user_cmd, path);
		cmd_index = find_command(user_cmd, cmd);
		if (cmd_index == 9)
		{
			// quit the program
			char *savepoint = "fssim_Sutton.txt";
			save(savepoint);
			break;
		}
		evaluate_command(cmd_index, path);
	}
}