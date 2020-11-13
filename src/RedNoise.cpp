#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <CanvasPoint.h>
#include <CanvasTriangle.h>
#include <Colour.h>
#include <ModelTriangle.h>
#include <TextureMap.h>
#include <TexturePoint.h>
#include <Utils.h>
#include <algorithm>

#define WIDTH 640
#define HEIGHT 480

std::vector<Colour> loadMtlFile(const std::string &filename) {
	std::vector<Colour> colours;

	std::ifstream inputStream(filename, std::ifstream::in);
	std::string nextLine;
	std::string name;

	while (std::getline(inputStream, nextLine)) {
		auto line = split(nextLine, ' ');
		
		if (line[0] == "newmtl") {
			name = line[1];
		} else if (line[0] == "Kd") {
			colours.push_back(Colour(
				name,
				(int)(std::stof(line[1]) * 255),
				(int)(std::stof(line[2]) * 255),
				(int)(std::stof(line[3]) * 255)
			));
		}
	}
	return colours;
}

std::vector<ModelTriangle> loadObjFile(const std::string &filename, float scale) {
	std::vector<glm::vec3> vertices;
	std::vector<TexturePoint> textureVertices;
	std::vector<ModelTriangle> faces;

	std::ifstream inputStream(filename, std::ifstream::in);
	std::string nextLine;
	std::vector<Colour> materials;
	Colour colour = Colour(255, 255, 255);

	while (std::getline(inputStream, nextLine)) {
		auto vector = split(nextLine, ' ');
		if (vector[0] == "mtllib") {
			materials = loadMtlFile(vector[1]);
		}
		else if (vector[0] == "usemtl") {
			for (int i=0; i<materials.size(); i++) {
				if (materials[i].name == vector[1]) {
					colour = materials[i];
					break;
				}
			}
		}
		else if (vector[0] == "v") {
			vertices.push_back(glm::vec3(
				std::stof(vector[1]) * scale,
				std::stof(vector[2]) * scale,
				std::stof(vector[3]) * scale
			));
		}
		else if (vector[0] == "vt") {
			textureVertices.push_back(TexturePoint(
				round(std::stof(vector[1]) * 480),
				round(std::stof(vector[2]) * 395)
			));
		}
		else if (vector[0] == "f") {
			auto v1 = split(vector[1], '/');
			auto v2 = split(vector[2], '/');
			auto v3 = split(vector[3], '/');
			auto triangle = ModelTriangle(
				vertices[std::stoi(v1[0]) - 1],
				vertices[std::stoi(v2[0]) - 1],
				vertices[std::stoi(v3[0]) - 1],
				colour
			);
			if (v1[1] != "") {
				triangle.texturePoints[0] = textureVertices[std::stoi(v1[1]) - 1];
				triangle.texturePoints[1] = textureVertices[std::stoi(v2[1]) - 1];
				triangle.texturePoints[2] = textureVertices[std::stoi(v3[1]) - 1];
			}

			faces.push_back(triangle);
		}
	}
	return faces;
}

float getNumberOfSteps(CanvasPoint from, CanvasPoint to) {
	return fmax(fmax(abs(to.x - from.x), abs(to.y - from.y)), 1);
}

void sortTriangleVertices(CanvasTriangle &triangle) {
	if (triangle[0].y > triangle[1].y) {
		std::swap(triangle[0], triangle[1]);
	}
	if (triangle[1].y > triangle[2].y) {
		std::swap(triangle[1], triangle[2]);
		if (triangle[0].y > triangle[1].y) {
			std::swap(triangle[0], triangle[1]);
		}	
	}
}

std::vector<TexturePoint> interpolatePoints(TexturePoint from, TexturePoint to, int steps) {
	std::vector<TexturePoint> TexturePoints;
	float xStep = (to.x - from.x) / (steps - 1);
	float yStep = (to.y - from.y) / (steps - 1);
	for (int i=0; i<steps; i++) {
		TexturePoints.push_back(TexturePoint(round(from.x + (i * xStep)),  round(from.y + (i * yStep))));
	}
	return TexturePoints;
}

std::vector<CanvasPoint> interpolatePoints(CanvasPoint from, CanvasPoint to, int steps) {
	std::vector<CanvasPoint> canvasPoints;
	float xStep = (to.x - from.x) / (steps - 1);
	float yStep = (to.y - from.y) / (steps - 1);
	float dStep = (to.depth - from.depth) / (steps - 1);
	auto texturePoints = interpolatePoints(from.texturePoint, to.texturePoint, steps);
	for (int i=0; i<steps; i++) {
		CanvasPoint point(round(from.x + (i * xStep)),  round(from.y + (i * yStep)), from.depth + (i * dStep));
		point.texturePoint = texturePoints[i];
		canvasPoints.push_back(point);
	}
	return canvasPoints;
}

void drawLine(DrawingWindow &window, std::vector<float> &depthBuffer, CanvasPoint from, CanvasPoint to, float numberOfSteps, std::vector<Colour> colours) {
	auto points = interpolatePoints(from, to, numberOfSteps + 1);
	for (float i=0.0; i<=numberOfSteps; i++) {
		auto x = points[i].x;
		auto y = points[i].y;
		if (0 < x && x < window.width && 0 < y && y < window.height) {
			int depthIndex = (y * window.width) + x;
			float pointDepth = 1 / -points[i].depth;
			if (pointDepth > depthBuffer[depthIndex]) {
				depthBuffer[depthIndex] = pointDepth;
				uint32_t c = (255 << 24) + (colours[i].red << 16) + (colours[i].green << 8) + colours[i].blue;
				window.setPixelColour(x, y, c);
			}
		}
	}
}

void drawLine(DrawingWindow &window, std::vector<float> &depthBuffer, CanvasPoint from, CanvasPoint to, Colour colour) {
	float numberOfSteps = getNumberOfSteps(from, to);
	drawLine(window, depthBuffer, from, to, numberOfSteps, std::vector<Colour>(numberOfSteps + 1, colour));
}

template <typename T>
void getRowStartsAndEnds(T p1, T p2, T p3, float height1, float height2, std::vector<T> &rowStarts, std::vector<T> &rowEnds) {
	rowStarts = interpolatePoints(p1, p2, height1);
	if (height2 > 1) {
		auto side2 = interpolatePoints(p2, p3, height2);
		rowStarts.pop_back();
		rowStarts.insert(rowStarts.end(), side2.begin(), side2.end());
	}
	rowEnds = interpolatePoints(p1, p3, height1 + height2 - 1);
}

template <typename T>
void getRowStartsAndEnds(CanvasTriangle triangle, float height1, float height2, std::vector<T> &rowStarts, std::vector<T> &rowEnds) {
	getRowStartsAndEnds(triangle[0], triangle[1], triangle[2], height1, height2, rowStarts, rowEnds);
}

CanvasTriangle generateTriangle(DrawingWindow &window) {
	float depth = (float(rand()) / float(RAND_MAX)) * -10;
	return CanvasTriangle(
		CanvasPoint(rand()%(window.width - 1), rand()%(window.height - 1), depth),
		CanvasPoint(rand()%(window.width - 1), rand()%(window.height - 1), depth),
		CanvasPoint(rand()%(window.width - 1), rand()%(window.height - 1), depth)
	);
}

CanvasTriangle generateTexturedTriangle() {
	float depth = (float(rand()) / float(RAND_MAX)) * -10;
	CanvasTriangle triangle = CanvasTriangle(CanvasPoint(160, 10, depth), CanvasPoint(300, 230, depth), CanvasPoint(10, 150, depth));
	triangle[0].texturePoint = TexturePoint(195, 5);
	triangle[1].texturePoint = TexturePoint(395, 380);
	triangle[2].texturePoint = TexturePoint(65, 330);
	return triangle;
}

void drawStrokedTriangle(DrawingWindow &window, std::vector<float> &depthBuffer, CanvasTriangle triangle, Colour colour) {
	drawLine(window, depthBuffer, triangle[0], triangle[1], colour);
	drawLine(window, depthBuffer, triangle[0], triangle[2], colour);
	drawLine(window, depthBuffer, triangle[1], triangle[2], colour);
}

void drawFilledTriangle(DrawingWindow &window, std::vector<float> &depthBuffer, CanvasTriangle triangle, Colour colour, bool outline) {
	sortTriangleVertices(triangle);

	// Work out the start and end xPos of each row of the triangle
	std::vector<CanvasPoint> starts, ends;
	getRowStartsAndEnds(triangle, triangle[1].y - triangle[0].y + 1, triangle[2].y - triangle[1].y + 1, starts, ends);

	// Draw the outline (if there is one) then fill in the triangle
	if (outline) drawStrokedTriangle(window, depthBuffer, triangle, Colour(255, 255, 255));

	for (int i=0; i<=triangle[2].y - triangle[0].y; i++) {
		starts[i].x = std::max(starts[i].x, 0.0f);
		ends[i].x = std::min(ends[i].x, float(window.width));
		drawLine(window, depthBuffer, starts[i], ends[i], colour);
	}
}

void drawTexturedTriangle(DrawingWindow &window, std::vector<float> &depthBuffer, CanvasTriangle triangle, TextureMap &texMap, bool outline) {
	sortTriangleVertices(triangle);

	float height1 = triangle[1].y - triangle[0].y + 1;
	float height2 = triangle[2].y - triangle[1].y + 1;
	std::vector<CanvasPoint> canvasStarts, canvasEnds;
	getRowStartsAndEnds(triangle, height1, height2, canvasStarts, canvasEnds);

	if (outline) drawStrokedTriangle(window, depthBuffer, triangle, Colour(255, 255, 255));

	for(int i=0; i<=height1 + height2 - 2; i++) {
		float numberOfSteps = getNumberOfSteps(canvasStarts[i], canvasEnds[i]);
		auto points = interpolatePoints(canvasStarts[i].texturePoint, canvasEnds[i].texturePoint, numberOfSteps + 1);

		std::vector<Colour> colours;
		for (float i=0.0; i<=numberOfSteps; i++) {
			uint32_t c = texMap.pixels[(round(points[i].y) * texMap.width) + round(points[i].x)];
			colours.push_back(Colour((c & 0xFF0000) >> 16, (c & 0xFF00) >> 8, (c & 0xFF)));
		}
		drawLine(window, depthBuffer, canvasStarts[i], canvasEnds[i], numberOfSteps, colours);
	}
}

template <typename T>
void rotateX(T &cam, float angle) {
	glm::mat3 rotationMatrix = glm::mat3(
		1.0, 0.0, 0.0,
		0.0, cos(angle), sin(angle),
		0.0, -sin(angle), cos(angle)
	);
	
	cam = cam * rotationMatrix;
}

template <typename T>
void rotateY(T &cam, float angle) {
	glm::mat3 rotationMatrix = glm::mat3(
		cos(angle), 0.0, -sin(angle),
		0.0, 1.0, 0.0,
		sin(angle), 0.0, cos(angle)
	);

	cam = cam * rotationMatrix ;
}

void reset(DrawingWindow &window, std::vector<float> &depthBuffer) {
	window.clearPixels();
	depthBuffer = std::vector<float>(window.height * window.width, 0);;
}

void draw(DrawingWindow &window, std::vector<float> &depthBuffer, glm::vec3 camPos, glm::mat3 camOri, float focalLength, std::vector<ModelTriangle> faces, TextureMap texMap) {
	reset(window, depthBuffer);

	float planeMultiplier = 250.0;
	for (int i=0; i<faces.size(); i++) {
		auto face = faces[i];
		CanvasTriangle triangle = CanvasTriangle();
		for (int j=0; j<face.vertices.size(); j++) {
			auto vertex = (face.vertices[j] - camPos) * camOri;
			triangle.vertices[j] = CanvasPoint(
				round(-(planeMultiplier * focalLength * (vertex[0] / vertex[2])) + (window.width / 2)),
				round((planeMultiplier * focalLength * (vertex[1] / vertex[2])) + (window.height / 2)),
				vertex[2]
			);
			triangle.vertices[j].texturePoint = face.texturePoints[j];
		}

		if (face.colour.name == "Cobbles") {
			drawTexturedTriangle(window, depthBuffer, triangle, texMap, false);
		}
		else {
			drawFilledTriangle(window, depthBuffer, triangle, face.colour, false);
		}
	}
}

glm::mat3 lookAt(glm::vec3 &camPos, glm::vec3 &target) {
	glm::vec3 forward = glm::normalize(camPos - target);
	glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), forward));
	glm::vec3 up = glm::normalize(glm::cross(forward, right));
	return glm::mat3(right, up, forward);
}

void update(DrawingWindow &window, glm::vec3 &camPos, glm::mat3 &camOri) {
	glm::vec3 origin(0, 0, 0);
	camOri = lookAt(camPos, origin);
}

void handleEvent(SDL_Event event, DrawingWindow &window, std::vector<float> &depthBuffer, TextureMap &texMap, glm::vec3 &camPos, glm::mat3 &camOri) {
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_LEFT) std::cout << "LEFT" << std::endl;
		else if (event.key.keysym.sym == SDLK_RIGHT) std::cout << "RIGHT" << std::endl;
		else if (event.key.keysym.sym == SDLK_UP) std::cout << "UP" << std::endl;
		else if (event.key.keysym.sym == SDLK_DOWN) std::cout << "DOWN" << std::endl;
		// else if (event.key.keysym.sym == SDLK_c) reset(window, depthBuffer);
		else if (event.key.keysym.sym == SDLK_u) drawStrokedTriangle(window, depthBuffer, generateTriangle(window), Colour(rand()%256, rand()%256, rand()%256));
		else if (event.key.keysym.sym == SDLK_f) drawFilledTriangle(window, depthBuffer, generateTriangle(window), Colour(rand()%256, rand()%256, rand()%256), true);
		else if (event.key.keysym.sym == SDLK_t) drawTexturedTriangle(window, depthBuffer, generateTexturedTriangle(), texMap, true);
		else if (event.key.keysym.sym == SDLK_w) camPos -= glm::vec3(0, 0, 0.5);
		else if (event.key.keysym.sym == SDLK_s) camPos -= glm::vec3(0, 0, -0.5);
		else if (event.key.keysym.sym == SDLK_z) camPos -= glm::vec3(0.5, 0, 0);
		else if (event.key.keysym.sym == SDLK_c) camPos -= glm::vec3(-0.5, 0, 0);
		else if (event.key.keysym.sym == SDLK_q) camPos -= glm::vec3(0, -0.5, 0);
		else if (event.key.keysym.sym == SDLK_e) camPos -= glm::vec3(0, 0.5, 0);
		else if (event.key.keysym.sym == SDLK_a) rotateY(camPos, 0.1);
		else if (event.key.keysym.sym == SDLK_d) rotateY(camPos, -0.1);
		else if (event.key.keysym.sym == SDLK_r) rotateX(camPos, 0.1);
		else if (event.key.keysym.sym == SDLK_v) rotateX(camPos, -0.1);
		else if (event.key.keysym.sym == SDLK_n) rotateY(camOri, 0.1);
		else if (event.key.keysym.sym == SDLK_m) rotateY(camOri, -0.1);
		else if (event.key.keysym.sym == SDLK_j) rotateX(camOri, 0.1);
		else if (event.key.keysym.sym == SDLK_k) rotateX(camOri, -0.1);
	} else if (event.type == SDL_MOUSEBUTTONDOWN) window.savePPM("output.ppm");
}

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	TextureMap texMap = TextureMap("models/texture.ppm");
	std::vector<float> depthBuffer = std::vector<float>(window.height * window.width, 0);
	
	float vertexScale = 1.0;

	std::vector<ModelTriangle> faces = loadObjFile("models/textured-cornell-box.obj", vertexScale);
	glm::vec3 camPos(0.0, 0.0, 10.0);
	glm::mat3 camOri(
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
		);
	float focalLength = 2.0;

	SDL_Event event;
	while (true) {
		// We MUST poll for events - otherwise the window will freeze !
		if (window.pollForInputEvents(event)) {
			handleEvent(event, window, depthBuffer, texMap, camPos, camOri);
			update(window, camPos, camOri);
			draw(window, depthBuffer, camPos, camOri, focalLength, faces, texMap);
		}
		// Need to render the frame at the end, or nothing actually gets shown on the screen !
		window.renderFrame();
	}
}
