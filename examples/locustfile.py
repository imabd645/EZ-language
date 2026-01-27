from locust import HttpUser, task

class MyUser(HttpUser):
    host = "http://localhost:5000"

    @task
    def index(self):
        self.client.get("/")
