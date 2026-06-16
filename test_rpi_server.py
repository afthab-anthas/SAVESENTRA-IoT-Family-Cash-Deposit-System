import pytest
from rpi_server import app


@pytest.fixture
def client():
    app.config["TESTING"] = True
    with app.test_client() as c:
        yield c


def test_index(client):
    res = client.get("/")
    assert res.status_code == 200
    assert b"up" in res.data


def test_deposit_missing_fields(client):
    res = client.post("/deposit", data={})
    assert res.status_code == 200


def test_predict_missing_goal(client):
    res = client.post("/predict", data={})
    assert res.status_code == 200
