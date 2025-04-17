#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <glob.h>

enum {
	Maxline = 4096,		// Tamanyo max linea de comandos
};

struct Shell {
	char line[Maxline];	// Linea de comandos
	char **tokens;		// Elementos de la linea de comandos
	char *fin;		// Fichero in
	char *fout;		// Fichero out
	char **here;		// Array con los argumentos que pasa HERE{document}
	int bg;			// Control de background
};
typedef struct Shell Shell;

int
get_pos(char *str, char ref)	// Posicion de la primera aparicion
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == ref) {
			return i;
		}
	}
	return -1;		// Si no hay
}

int
get_num(char *str, char ref)	// Devuelve el numero de caracteres ref que hay en str
{
	int i;
	int count = 0;

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == ref) {
			count++;
		}
	}
	return count;
}

void
modify_str(char **str, int ini, int end)	// Desplazar chars del string, para "eliminar" parte del string
{
	int i;

	for (i = end; (*str)[i] != '\0'; i++) {
		(*str)[ini] = (*str)[i];	// Mover los caracteres si lo hay, de izq a der
		ini++;
	}
	(*str)[ini] = '\0';	// Terminar string en '\0'
}

int
get_pos_substr(char *str, char *sub)	// Devuelve la posicion inicial de un substring dentro de un string
{
	char *pos = strstr(str, sub);	// Devuelve un puntero a la primera aparicion del subtring

	if (pos != NULL) {
		return pos - str;	// Devuelve la posicion en la que empieza sub en str
	}
	return -1;		// Si no esta sub en str
}

int
get_num_substr(char *str, char *sub)	// Devuelve cuantos subtrings especificos hay en el string
{
	int count = 0;
	char *pos = str;

	while ((pos = strstr(pos, sub)) != NULL) {	// strstr devuelve la posicion del primer char encontrado
		count++;
		pos += strlen(sub);	// Avanzar len del substr, a la posicion del primer char encontrado
	}
	return count;
}

int
pos_substr_arr(char **array, char *str)	// Ver si el array, contiene str en algun elemento
{
	int i = 0;
	int pos = -10;		// Si no hay ninguno, devuelve pos = -10
	int total = 0;

	while (array[i] != NULL) {
		if (strstr(array[i], str) != NULL) {
			total++;
			if (total == 1) {
				pos = i;
			}
		}
		i++;
	}
	if (total <= 1) {	// Solo devuelve la posicion donde esta, si solo se encuentra en un string del array
		return pos;
	}
	return -1;		// Si hay mas de 1, devuelve -1
}

int
get_num_total(char **array, char ref)	// Igual que get_num, pero para todos los str del array
{
	int i;
	int count = 0;

	for (i = 0; array[i] != NULL; i++) {
		count += get_num(array[i], ref);
	}
	return count;
}

int
get_length(char **array)	// Devuelve la longitud de un array
{
	int len = 0;

	while (array[len] != NULL) {
		len++;
	}
	return len;
}

void
get_free_pos(char **array, int pos)	// Liberar posicion de un array 
{
	int i;
	int len = get_length(array);

	if (array[pos] != NULL) {
		free(array[pos]);
		array[pos] = NULL;
	}
	for (i = pos; i < len - 1; i++) {
		array[i] = array[i + 1];
	}
	array[len - 1] = NULL;	// Terminar el array en NULL
}

int
pos_char_array(char **array, char ref)	// Devuelve el num del primer string que lo contiene
{
	int i;

	for (i = 0; array[i] != NULL; i++) {
		if (get_num(array[i], ref) > 0) {
			return i;
		}
	}
	return -1;		// Si no hay
}

void
get_free_arr(char **array)	// Liberar array entero
{
	int i;

	if (array != NULL) {
		for (i = 0; array[i] != NULL; i++) {
			free(array[i]);
		}
		free(array);
	}
}

void
clean_out(Shell **sh)		// Limpiar varible shell entera
{
	if (*sh != NULL) {
		get_free_arr((*sh)->tokens);
		(*sh)->tokens = NULL;
		(*sh)->line[0] = '\0';
		(*sh)->bg = 0;
		if ((*sh)->fin != NULL) {
			free((*sh)->fin);
			(*sh)->fin = NULL;
		}
		if ((*sh)->fout != NULL) {
			free((*sh)->fout);
			(*sh)->fout = NULL;
		}
		get_free_arr((*sh)->here);
		(*sh)->here = NULL;
	}
}

void
modify_result(int status)	// OPCIONAL II: Modificar variable de entorno result, segun la salida del ultimo comando
{
	char result[10];	// Donde se va a pasar el estado a string, max 9 digitos

	if (snprintf(result, sizeof(result), "%d", status) < 0) {
		fprintf(stderr, "error: snprintf failed\n");
		modify_result(status);
	}
	if (setenv("result", result, 1) == -1) {
		warnx("error: setting result failed\n");
		modify_result(status);
	}			// Recursividad: si falla algo de la funcion, se vuelve a llamar a la funcion para poder modificar la variable de entorno
}

void
repeat(Shell **sh, int i)	// Inicializar shell para repetir bucle
{
	if (i == 1) {
		warn(NULL);
		modify_result(1);
	}
	clean_out(sh);
	printf("$> ");
}

void
set_shell(Shell **sh)
{
	(*sh)->line[0] = '\0';
	(*sh)->tokens = NULL;
	(*sh)->fin = NULL;
	(*sh)->fout = NULL;
	(*sh)->here = NULL;
	(*sh)->bg = 0;
	modify_result(0);
}

int
tokenize(char *buf, char ***tokens, char *ref)	// Crear array a partir de un string, separando por los caracteres indicados
{
	char *buf_aux = NULL;
	char *token = NULL;
	char *saveptr;
	int i = 0;

	buf_aux = strdup(buf);	// Para no modificar el buffer original
	if (!buf_aux) {
		return 1;
	}
	token = strtok_r(buf_aux, ref, &saveptr);	// Filtra separando las cadenas que poseen estos chars
	while (token != NULL) {
		*tokens = realloc(*tokens, (i + 1) * sizeof(char *));
		if (!(*tokens)) {
			return 1;
		}
		(*tokens)[i] = strdup(token);
		if (!(*tokens)[i]) {
			return 1;
		}
		token = strtok_r(NULL, ref, &saveptr);	// Ahora el control del buf es saveptr
		i++;
	}
	*tokens = realloc(*tokens, (i + 1) * sizeof(char *));
	if (!(*tokens)) {
		return 1;
	}
	(*tokens)[i] = NULL;	// Terminar el array en NULL
	free(buf_aux);
	buf_aux = NULL;
	return 0;
}

int
val_env(Shell **sh)		// Sustituir por el valor de la variable de entorno, si empieza por $ y existe
{
	char *token = NULL;
	char *var_env = NULL;
	char *new_token = NULL;
	int pos;
	int i;

	for (i = 0; (*sh)->tokens[i] != NULL; i++) {
		token = strdup((*sh)->tokens[i]);	// Para no hacerlo sobre los datos originales
		if (!token) {
			warn(NULL);
			modify_result(1);
			return 1;
		}
		if (get_num(token, '$') > 1) {
			fprintf(stderr,
				"error: only 1 environment var allowed per token\n");
			modify_result(1);
			free(token);
			return 1;
		}
		pos = get_pos(token, '$');	// Ver en que posicion esta '$' y sustituir a partir de ahi
		if (get_num(token, '$') == 1 && strlen(token + pos) > 1) {
			var_env = getenv(token + pos + 1);
			if (!var_env) {
				fprintf(stderr,
					"error: var %s does not exist\n",
					token + pos);
				modify_result(1);
				free(token);
				return 1;
			}
			token[pos] = '\0';	// Eliminar la parte donde se encontraba la variable de entorno
			new_token =
			    (char *)malloc(strlen(token) + strlen(var_env) + 1);
			if (!new_token) {	// Reservar memoria para que quepa el nuevo token entero
				free(token);
				warn(NULL);
				modify_result(1);
				return 1;
			}
			strcpy(new_token, token);
			strcat(new_token, var_env);	// Formar el nuevo token, sustituyendo la variable de entorno
			free((*sh)->tokens[i]);	// Liberar memoria de la info que habia antes
			(*sh)->tokens[i] = NULL;
			(*sh)->tokens[i] = strdup(new_token);	// Cambiar el puntero de la memoria al nuevo con la sustitucion
			free(new_token);
			new_token = NULL;
			if (!((*sh)->tokens[i])) {
				free(token);
				warn(NULL);
				modify_result(1);
				return 1;
			}
		}
		free(token);
		token = NULL;
	}
	return 0;
}

int
globbing(Shell **sh)		// OPCIONAL III: Aplicar globbing(*) para coincidencias en cadenas
{
	int glob_status;
	glob_t glob_results;
	int glob_flags = 0;
	int i;
	int j;
	int k;

	for (i = 0; (*sh)->tokens[i] != NULL; i++) {
		glob_status =
		    glob((*sh)->tokens[i], glob_flags, NULL, &glob_results);
		switch (glob_status) {
		case 0:	// Se han encontrado coincidencias
			free((*sh)->tokens[i]);	// Eliminar el token original
			(*sh)->tokens[i] = NULL;
			(*sh)->tokens[i] = strdup(glob_results.gl_pathv[0]);	// Sustituir por el primer resultado del globbling
			if (!(*sh)->tokens[i]) {
				globfree(&glob_results);
				return 1;
			}
			for (j = 1; j < glob_results.gl_pathc; j++) {	// Si al hacer el globbing, se generan mas tokens y hay que agregarlos
				(*sh)->tokens =
				    realloc((*sh)->tokens,
					    (get_length((*sh)->tokens) +
					     2) * sizeof(char *));
				if (!(*sh)->tokens) {
					globfree(&glob_results);
					return 1;
				}
				for (k = get_length((*sh)->tokens); k > i + 1; k--) {	// Desplazar tokens a la derecha e insertar el nuevo
					(*sh)->tokens[k] = (*sh)->tokens[k - 1];
				}
				(*sh)->tokens[i + 1] = strdup(glob_results.gl_pathv[j]);	// Insertar nuevo token
				if (!(*sh)->tokens[i + 1]) {
					globfree(&glob_results);
					return 1;
				}
				i++;	// Para evaluar siguiente token
			}
			break;
		case GLOB_NOMATCH:	// No se han encontrado coincidencias, se deja * donde estuviese
			break;
		default:	// Salio con error la funcion glob
			globfree(&glob_results);
			return 1;
		}
		globfree(&glob_results);
	}
	return 0;		// Si se realiza el globbing correctamente
}

void
val_bg(Shell **sh)		// Comprobar si en el ultimo token, al final, se encuentra el indicador de background &
{
	int len_toks = get_length((*sh)->tokens);
	int len_last_tok = strlen((*sh)->tokens[len_toks - 1]);

	len_toks = get_length((*sh)->tokens);
	if ((*sh)->tokens[len_toks - 1][len_last_tok - 1] == '&') {
		(*sh)->bg = 1;
		if (len_last_tok == 1) {	// Si solo esta &, se libera espacio
			get_free_pos((*sh)->tokens, len_toks - 1);
		} else {	// Si hay mas caracteres, solo se borra el ultimo, que es el &
			(*sh)->tokens[len_toks - 1][len_last_tok - 1] = '\0';
		}
	}
}

int
val_heredoc(Shell **sh, int ntok)	// Opcional I: Identificar documento dentro de HERE{document}, y guardarlo
{
	int nheres = get_num_substr((*sh)->tokens[ntok], "HERE{");
	int poshere = get_pos_substr((*sh)->tokens[ntok], "HERE{");	// Posicion donde se encuentra la H en el token
	int posend;
	int here_size;
	int m = 0;
	int i;

	if (nheres > 1) {
		fprintf(stderr, "error: only 1 HERE{document} allowed\n");
		return 1;
	}
	posend = get_length((*sh)->tokens) - 1;
	if ((*sh)->tokens[posend][strlen((*sh)->tokens[posend]) - 1] != '}') {
		fprintf(stderr, "error: '}' need to be at the end\n");
		return 1;
	}
	(*sh)->tokens[posend][strlen((*sh)->tokens[posend]) - 1] = '\0';	// Borar '}' para copiar informacion

	here_size = get_length((*sh)->tokens) - ntok;	// Numero de tokens que se van a guardar
	(*sh)->here = calloc(here_size + 1, sizeof(char *));	// Reservar memoria para dichos tokens
	if (!(*sh)->here) {
		warn(NULL);
		return 1;
	}
	modify_str(&(*sh)->tokens[ntok], poshere, poshere + 5);
	if ((*sh)->tokens[ntok][poshere] != '\0') {
		(*sh)->here[0] = strdup((*sh)->tokens[ntok] + poshere);	// Copiar info a partir de la primera posicion del HERE{
		if (!(*sh)->here[0]) {
			warn(NULL);
			return 1;
		}
	} else {
		m++;		// Para controlar que el primer token que se tiene que guardar es el siguiente
	}
	if ((*sh)->tokens[ntok][0] == '\0') {	// Liberar primer token si esta vacio
		free((*sh)->tokens[ntok]);
		(*sh)->tokens[ntok] = NULL;
	}
	for (i = 1; (*sh)->tokens[ntok + i] != NULL; i++) {
		(*sh)->here[i - m] = strdup((*sh)->tokens[ntok + i]);
		if (!(*sh)->here[i - m]) {	// Guardar token a token, a partir del HERE{ y liberar tokens
			warn(NULL);
			return 1;
		}
		free((*sh)->tokens[ntok + i]);
		(*sh)->tokens[ntok + i] = NULL;
	}
	(*sh)->here[i - m] = NULL;	// Terminar array en NULL
	if ((*sh)->here[0][0] == '\0') {
		fprintf(stderr, "error: doc in HERE{} is empty\n");
		return 1;
	}
	return 0;		// HERE{document} realizado sin problemas
}

int
val_std(Shell **sh, char *path, char ref, int pos, int pos2)	// Comprobar si son ficheros aptos
{
	if (get_pos(path, ref) >= 0 && pos2 > pos) {	// Ver si hay redireccion nueva a la derecha, solo si esta todo en el mismo token
		path[pos2 - pos - 1] = '\0';	// Eliminar la siguiente redireccion, si la hay
	}
	if (ref == '<') {
		if (access(path, R_OK) == 0) {	// Comprobar si se puede leer para ser la entrada
			(*sh)->fin = strdup(path);
			return 0;
		}
	} else {
		if (access(path, W_OK) == 0) {	// Comprobar si se puede escribir para ser la salida
			(*sh)->fout = strdup(path);
			return 0;
		}
	}
	return 1;		// Si no nos ha dejado acceder al fichero
}

int
try_err_path(Shell **sh, char **path, char **tok, char ref, int pos, int pos2)
{
	if (!(*path) || val_std(sh, *path, ref, pos, pos2) == 1) {
		free(*tok);
		if (*path != NULL) {
			free(*path);
		}
		fprintf(stderr, "error: access to file failed\n");
		return 1;
	}
	return 0;
}

int
get_std(Shell **sh, int pos_tok, char ref, char ref2)	// Identificar fichero de redireccionamiento
{
	char *tok = NULL;
	char *path = NULL;
	int pos = 0;
	int pos2 = 0;

	tok = strdup((*sh)->tokens[pos_tok]);
	if (!tok) {
		warn(NULL);
		return 1;
	}
	pos = get_pos(tok, ref);
	if (strlen(tok) == 1 || pos == strlen(tok) - 1) {	// ref solo o al final, path es el siguiente token
		if ((*sh)->tokens[pos_tok + 1] == NULL) {
			free(tok);
			fprintf(stderr,
				"error: missing path after redirection\n");
			return 1;
		}
		path = strdup((*sh)->tokens[pos_tok + 1]);
		pos2 = get_pos(path, ref2);	// Para ver si hay otro direccionamiento en el siguiente token, donde esta el path
		if (pos2 == 0) {	// Salir con error porque hay dos redireccionamientos seguidos
			free(tok);
			if (path != NULL) {
				free(path);
			}
			fprintf(stderr,
				"error: missing path after redirection\n");
			return 1;
		} else if (pos2 > 0) {
			path[pos2] = '\0';
		}
		if (try_err_path(sh, &path, &tok, ref, pos, pos2) == 1) {
			return 1;
		}
		if (pos2 > 0) {	// Eliminar path ya utilizado en el token
			modify_str(&(*sh)->tokens[pos_tok + 1], 0, pos2);
		} else {
			get_free_pos((*sh)->tokens, pos_tok + 1);
		}
		if (strlen(tok) == 1) {	// Eliminar las redirecciones de los tokens, para no confundir con comandos
			get_free_pos((*sh)->tokens, pos_tok);
		} else {
			(*sh)->tokens[pos_tok][strlen(tok) - 1] = '\0';
		}

	} else {		// Suponer que el path esta en el mismo token, desde la redireccion hasta el final del token
		pos2 = get_pos(tok, ref2);	// Para ver si hay otra redireccion en el mismo token
		if (pos2 == pos + 1) {
			free(tok);
			fprintf(stderr,
				"error: missing path after redirection\n");
			return 1;
		}
		path = strdup(tok + pos + 1);	// Path para la redireccion
		if (try_err_path(sh, &path, &tok, ref, pos, pos2) == 1) {
			return 1;
		}
		if (pos2 > pos) {
			modify_str(&(*sh)->tokens[pos_tok], pos, pos2);
		} else {
			(*sh)->tokens[pos_tok][pos] = '\0';	// Eliminar la redireccion y el path del propio token, manteniendo el siguiente si lo hay
		}
		if ((*sh)->tokens[pos_tok][0] == '\0') {
			get_free_pos((*sh)->tokens, pos_tok);	// Si se ha borrado todo el string, liberar espacio
		}
	}
	free(tok);
	free(path);
	return 0;		// Si se ha hecho bien la redireccion
}

int
redirect(Shell **sh, char ref)	// Guardar paths de redireccionamiento, si las hay
{
	int pos_tok;
	char ref2;

	if (ref == '>') {	// Declara redireccion contraria, para saber si hay mas redirecciones donde se evalua
		ref2 = '<';
	} else {
		ref2 = '>';
	}
	switch (get_num_total((*sh)->tokens, ref)) {
	case 0:		// No se cambia
		return 0;
	case 1:		// Si se cambia
		pos_tok = pos_char_array((*sh)->tokens, ref);
		return get_std(sh, pos_tok, ref, ref2);
	default:		// Error, no puede haber mas redirecciones, solo 1 (in y/o out)
		fprintf(stderr, "error: only 1 same redirection %c allowed\n", ref);
		return 1;
	}
}

int
ifresult(Shell **sh, int len_toks, char ref)	// OPCIONAL II: Comandos builtin, comprueba estatus de los ultimos comando ejecutados
{
	char *result = getenv("result");
	int status;

	if (len_toks < 2) {
		fprintf(stderr, "error: more than 1 argument needed\n");
		return 1;
	}
	status = atoi(result);	// Pasar resultado de string a entero
	if (ref == '<') {	// comando ifok => estado anterior 0, se ejecuta comando (no fallo)
		if (status < 1) {
			get_free_pos((*sh)->tokens, 0);
			return 0;
		}
	} else {		// comando ifnot => estado anterior > 0, se ejecuta comando (si fallo)
		if (status >= 1) {
			get_free_pos((*sh)->tokens, 0);
			return 0;
		}
	}
	fprintf(stderr,
		"error: last status does not allow command to be executed\n");
	return 1;		// Si no lo cumple, no se ejecuta el comando
}

int
new_var(Shell **sh)		// Comprobar si es la estructura a=b, y declarar varibale de entorno
{
	char **tok_aux = NULL;
	char *line_aux = NULL;

	line_aux = strdup((*sh)->tokens[0]);	// Lo copiamos para no modificar tokens
	if (!line_aux) {
		return 1;
	}
	if (tokenize(line_aux, &tok_aux, "=") == 1) {
		if (tok_aux != NULL) {
			get_free_arr(tok_aux);
		}
		free(line_aux);
		return 1;
	}
	free(line_aux);
	if (get_length(tok_aux) == 2) {	// Definir var entorno => tok_aux[0]=tok_aux[1]
		if (setenv(tok_aux[0], tok_aux[1], 1) == -1) {
			get_free_arr(tok_aux);
			return 1;
		}
		get_free_arr(tok_aux);
		modify_result(0);
		return 0;	// Devuelve 0, cuando se hace la sustitucion
	}
	get_free_arr(tok_aux);
	fprintf(stderr, "error: incomplete expression, need to be a=b\n");
	modify_result(1);
	return 0;		// Devuelve 0 tambien, si no cumple con la especificacion, con mensaje de error
}

int
val_dir(char *dir)		// Verifica si el path que se le pasa, pertenece a un directorio o no
{
	struct stat path_stat;

	if (stat(dir, &path_stat) != 0) {
		return 1;	// No es un directorio o el path no existe
	}
	return S_ISDIR(path_stat.st_mode);	// Comprobar si el path es de un directorio
}

int
cd(Shell *sh, int len_toks)	// Builtin del shell, redirigir directorio actual a otro
{
	if (len_toks == 1) {	// Si no se le pasa path, se cambia por defecto a HOME
		if (chdir(getenv("HOME")) == -1) {
			return 1;
		}
	} else {		// Sino, cambiar al path que se le passa como argumento
		if (!val_dir(sh->tokens[1])) {
			return 1;
		}
		if (chdir(sh->tokens[1]) == -1) {
			return 1;
		}
	}
	modify_result(0);
	return 0;		// Si el cambio de directorio fue exitoso
}

void
val_redir(Shell **sh, int std)
{
	char *nom_file = NULL;
	FILE *file;
	int fd;
	char mode[2];

	if (std == 1) {		// Salida estandar (stdout = 1)
		if ((*sh)->fout != NULL) {
			nom_file = strdup((*sh)->fout);
			if (!nom_file) {
				err(EXIT_FAILURE, NULL);
			}
		}
		strcpy(mode, "w");
	} else {		// Entrada estandar (stdin = 0)
		if ((*sh)->fin != NULL) {
			nom_file = strdup((*sh)->fin);
			if (!nom_file) {
				err(EXIT_FAILURE, NULL);
			}
		}
		strcpy(mode, "r");
	}
	if (!nom_file && (*sh)->bg == 1 && std == 0) {
		nom_file = strdup("/dev/null");	// Redirigir stdin a NULL para no leer de la shell si esta en background
		if (!nom_file) {
			err(EXIT_FAILURE, NULL);
		}
	}
	if (nom_file != NULL) {
		file = fopen(nom_file, mode);
		free(nom_file);
		nom_file = NULL;
		if (!file) {
			err(EXIT_FAILURE, NULL);
		}
		fd = fileno(file);	// Pasar stream a descriptor de fichero
		if (fd == -1) {
			fclose(file);
			err(EXIT_FAILURE, NULL);
		}
		if (dup2(fd, std) == -1) {	// Duplicar el descriptor de fichero en el estandar
			fclose(file);
			err(EXIT_FAILURE, NULL);
		}
		fclose(file);
	}
}

int
here_pipe(Shell **sh, char *path)	// OPCIONAL 1: Redirigir entrada estandar con pipe
{
	int pipehere[2];
	pid_t pid;
	int status;
	int i;

	if (pipe(pipehere) == -1) {	// Crear pipe entre hijo y padre
		return 1;
	}
	pid = fork();
	switch (pid) {
	case -1:		// Error al hacer fork
		close(pipehere[0]);
		close(pipehere[1]);
		return 1;
	case 0:		// Proceso hijo
		close(pipehere[1]);	// Cerrar escritura del pipe
		if (dup2(pipehere[0], 0) == -1) {	// Dirigir entrada estandar en la lectura del pipe
			close(pipehere[0]);
			err(EXIT_FAILURE, NULL);
		}
		close(pipehere[0]);	// Cerrar descriptor del pipe que falta al terminar de utilizarlo
		execv(path, (*sh)->tokens);	// Ejecutar path del comando con los argumentos en los tokens
		err(EXIT_FAILURE, NULL);
	default:		// Proceso padre
		close(pipehere[0]);	// Cerrar lectura del pipe
		for (i = 0; (*sh)->here[i] != NULL; i++) {	// Pasarle al hijo por el pipe, cada token de here
			if (write
			    (pipehere[1], (*sh)->here[i],
			     strlen((*sh)->here[i] + 1)) == -1) {
				close(pipehere[1]);
				return 1;
			}
			if (write(pipehere[1], " ", 1) == -1) {	// Anyadir un espacio despues de cada token de here
				close(pipehere[1]);
				return 1;
			}
		}
		if (write(pipehere[1], "\n", 1) == -1) {	// Anyadir un salto de linea al final
			close(pipehere[1]);
			return 1;
		}
		close(pipehere[1]);	// Cerrar escritura del pipe al terminar de utilizarlo
		waitpid(pid, &status, 0);	// Esperar a que termine el proceso hijo
		modify_result(WEXITSTATUS(status));
		free(path);
		path = NULL;
	}
	return 0;
}

int
find_path(Shell **sh, char **path)	// Encontrar el path al comando mediante la variable path
{
	char **dirs = NULL;	// Array, para guardar los directorios de la variable PATH
	char *path_env = getenv("PATH");
	size_t path_len = 0;
	int i;

	if (tokenize(path_env, &dirs, ":") == 1) {	// Tokenizar los directorios separados por ':'
		return 1;
	}
	for (i = 0; dirs[i] != NULL; i++) {
		path_len = strlen(dirs[i]) + strlen((*sh)->tokens[0]) + 2;
		*path = (char *)malloc(path_len);
		if (!(*path)) {
			get_free_arr(dirs);
			return 1;
		}
		snprintf(*path, path_len, "%s/%s", dirs[i], (*sh)->tokens[0]);
		if (access(*path, X_OK) == 0) {	// Ejecutable encontrado
			get_free_arr(dirs);
			return 0;
		}
		free(*path);
		*path = NULL;
	}
	get_free_arr(dirs);
	return 0;
}

int
run(Shell **sh)
{
	pid_t pid;
	char *path = NULL;
	int status;

	if (find_path(sh, &path) == 1) {
		return 1;
	}
	if (!path) {
		fprintf(stderr, "error: command not found\n");
		modify_result(1);
		return 0;
	}
	if ((*sh)->here != NULL) {
		return here_pipe(sh, path);
	}
	pid = fork();
	switch (pid) {
	case -1:		// Error al hacer el fork
		free(path);
		return 1;
	case 0:		// Proceso hijo
		val_redir(sh, 0);
		val_redir(sh, 1);
		execv(path, (*sh)->tokens);	// Ejecutar path del comando con los argumentos en los tokens
		err(EXIT_FAILURE, NULL);
	default:		// Proceso padre
		if ((*sh)->bg == 0) {	// Si no es en segundo plano, esperar al hijo
			waitpid(pid, &status, 0);
			modify_result(WEXITSTATUS(status));
		} else {
			printf("&: running command in backgroud\n");
			if (waitpid(pid, &status, WNOHANG) > 0) {	// Dejara correr el comando en segundo plano
				modify_result(WEXITSTATUS(status));	// Cuando termine cambiar la variable de entorno result
			}
		}
	}
	free(path);
	path = NULL;
	return 0;
}

int
main(int argc, char *argv[])
{
	Shell *sh = NULL;
	int len_toks;
	int op1;

	sh = (Shell *)malloc(sizeof(Shell));	// Inicializar nueva shell
	if (!sh) {
		err(EXIT_FAILURE, NULL);
	}
	set_shell(&sh);
	printf("$> ");
	while (fgets(sh->line, Maxline, stdin) != NULL) {	// Leer una linea y almacenarla
		if (sh->line[0] == '\n') {
			repeat(&sh, 0);
			continue;
		}
		sh->line[strlen(sh->line) - 1] = '\0';	// Borrar '\n' del final, para no confundir en ejecucion
		len_toks = 0;	// Inicializar len_toks a 0 de nuevo
		if (tokenize(sh->line, &sh->tokens, " \t") == 1) {	// Tokenizar por espacios la linea de comandos
			repeat(&sh, 1);
			continue;
		}
		if (val_env(&sh) == 1) {	// Sustituir variables de entorno si las hay, solo aisladas
			modify_result(1);
			repeat(&sh, 0);
			continue;
		}
		if (globbing(&sh) == 1) {	// Ver coincidencias con ficheros (*)
			repeat(&sh, 1);
			continue;
		}
		val_bg(&sh);	// Ver si se quiere ejecutar en background
		op1 = pos_substr_arr(sh->tokens, "HERE{");
		if (op1 >= -1) {
			if (op1 == -1) {
				fprintf(stderr,
					"error: only 1 HERE{...} allowed\n");
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
			if (sh->bg == 1 || get_num_total(sh->tokens, '<') > 0
			    || get_num_total(sh->tokens, '>') > 0) {
				fprintf(stderr,
					"error: no redirections allowed with HERE{...}\n");
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
			if (val_heredoc(&sh, op1) == 1) {
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
		} else if (redirect(&sh, '<') == 1 || redirect(&sh, '>') == 1) {	// Ver redirecciones posibles, y guardar fichero para salida y entrada estandar
			modify_result(1);
			repeat(&sh, 0);
			continue;
		}
		len_toks = get_length(sh->tokens);	// Cuantos tokens hay despues de eliminar redirecciones, etc
		if (strcmp(sh->tokens[0], "ifok") == 0) {	// Builtin ifok
			if (ifresult(&sh, len_toks, '<') == 1) {
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
		} else if (strcmp(sh->tokens[0], "ifnot") == 0) {	// Builtin ifnot
			if (ifresult(&sh, len_toks, '>') == 1) {
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
		}
		if (strcmp(sh->tokens[0], "exit") == 0) {
			if (len_toks != 1) {
				fprintf(stderr,
					"error: to exit shell, type only 'exit'\n");
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
			break;	// Cerrar shell
		} else if (len_toks == 1 && get_num(sh->tokens[0], '=') == 1) {	// Comprobar estructura, para poder declarar nueva varible de entorno
			repeat(&sh, new_var(&sh));
			continue;
		} else if (strcmp(sh->tokens[0], "cd") == 0) {	// Comando builtin de cd, cambiar de directorio en la shell
			if (len_toks > 2) {
				fprintf(stderr,
					"error: cd only works with 1 dictory path\n");
				modify_result(1);
				repeat(&sh, 0);
				continue;
			}
			repeat(&sh, cd(sh, len_toks));	// Cambiar directorio, (o fallar con errno), y repetir
			continue;
		} else {	// Si no encaja con nada anterior, se asume que hay que correr comandos
			repeat(&sh, run(&sh));
			continue;
		}
	}
	clean_out(&sh);		// Finaliza la shell
	free(sh);
	sh = NULL;
	exit(EXIT_SUCCESS);
}
