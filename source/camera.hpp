class Camera {
        private:
            glm::vec3 position;
            glm::vec3 front;
            glm::vec3 up;
            float fov;
        public:
            Camera(glm::vec3 pos, glm::vec3 front, glm::vec3 up, float fov) : position(pos), front(front), up(up), fov(fov) {}

            void translate(glm::vec3 offset) { position += offset; }

            void setPosition(glm::vec3 pos) { position = pos; }
            void setFront(glm::vec3 f) { front = f; }
            void setUp(glm::vec3 u) { up = u; }
            void setFov(float f) { fov = f; }

            float getFov() { return fov; }

            glm::vec3 getPosition() { return position; }
            glm::vec3 getFront() { return front; }
            glm::vec3 getUp() { return up; }
            glm::mat4 getViewMatrix() { return glm::lookAt(position, position + front, up); }
            glm::mat4 getProjectionMatrix(float aspectRatio) { return glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f); }
            glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane, float farPlane) { return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane); }
};